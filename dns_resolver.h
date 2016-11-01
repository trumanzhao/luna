/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <thread>
#include <string>
#include <list>

struct dns_request_t
{
	std::string node;
	std::string service;
	std::function<void(struct addrinfo*)> dns_cb;
	std::function<void(const char*)> err_cb;
};

class dns_resolver
{
public:
	dns_resolver();
	~dns_resolver();

	void request(dns_request_t* req);
	void update();

private:
	void work();
	void resolve(dns_request_t* req);

	std::mutex m_req_lock;
	std::list<dns_request_t*> m_reqs;
	volatile bool m_run = true;
	std::thread m_thread;
	std::mutex m_rep_lock;
	std::list<std::function<void()>> m_reps;
};
