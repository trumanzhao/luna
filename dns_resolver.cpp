#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#ifdef __linux
#include <sys/epoll.h>
#endif
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#if defined(__linux) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <algorithm>
#include <string.h>
#include "tools.h"
#include "dns_resolver.h"

static const int DNS_THREAD_COUNT = 2;

dns_resolver::dns_resolver()
{
	m_thread = std::thread(&dns_resolver::work, this);
}

dns_resolver::~dns_resolver()
{
	m_run = false;
	m_thread.join();
	for_each(m_reqs.begin(), m_reqs.end(), [this](auto req) { delete req; });
}

void dns_resolver::request(dns_request_t* req)
{
	std::lock_guard<std::mutex> g(m_req_lock);
	m_reqs.push_back(req);
}

// 用户可以在任何时刻close,但是要m_dns_resolved完成后才会真的删除对象
void dns_resolver::update()
{
	m_rep_lock.lock();
	auto reps = std::move(m_reps);
	m_rep_lock.unlock();
	for_each(reps.begin(), reps.end(), [](auto& rep) { rep(); });
}

void dns_resolver::work()
{
	while (m_run)
	{
		m_req_lock.lock();
		auto reqs = std::move(m_reqs);
		m_req_lock.unlock();

		if (reqs.empty())
		{
			sleep_ms(10);
			continue;
		}

		for_each(reqs.begin(), reqs.end(), [this](auto req) { resolve(req); });
	}
}

void dns_resolver::resolve(dns_request_t* req)
{
	addrinfo hints;
	struct addrinfo* addr = nullptr;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo(req->node.c_str(), req->service.c_str(), &hints, &addr);
	if (ret != 0)
	{
#if defined(__linux) || defined(__APPLE__)
		std::string err = gai_strerror(ret);
#endif

#if _MSC_VER
		std::string err = gai_strerrorA(ret);
#endif

		std::lock_guard<std::mutex> g(m_rep_lock);
		m_reps.push_back([=, cb=std::move(req->err_cb)](){ cb(err.c_str()); });
	}
	else
	{
		std::lock_guard<std::mutex> g(m_rep_lock);
		m_reps.push_back([=, cb = std::move(req->dns_cb)](){ cb(addr); freeaddrinfo(addr); });
	}
	delete req;
}
