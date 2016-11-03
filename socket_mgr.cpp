/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
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
#include "tools.h"
#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"
#include "socket_stream.h"
#include "socket_listener.h"

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

socket_manager::socket_manager()
{
#ifdef _MSC_VER
	WORD    wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	WSAStartup(wVersion, &wsaData);
#endif
}

socket_manager::~socket_manager()
{
	for (auto& node : m_objects)
	{
		delete node.second;
	}

#ifdef _MSC_VER
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_handle);
		m_handle = INVALID_HANDLE_VALUE;
	}
	WSACleanup();
#endif

#ifdef __linux
	if (m_handle != -1)
	{
		close(m_handle);
		m_handle = -1;
	}
#endif

#ifdef __APPLE__
	if (m_handle != -1)
	{
		close(m_handle);
		m_handle = -1;
	}
#endif
}

bool socket_manager::setup(int max_connection)
{
#ifdef _MSC_VER
	m_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_handle == INVALID_HANDLE_VALUE)
		return false;
#endif

#ifdef __linux
	m_handle = epoll_create(max_connection);
	if (m_handle == -1)
		return false;
#endif

#ifdef __APPLE__
	m_handle = kqueue();
	if (m_handle == -1)
		return false;
#endif

	m_max_connection = max_connection;
	m_events.resize(max_connection);

	return true;
}

void socket_manager::wait(int timeout)
{
#ifdef _MSC_VER
	ULONG event_count = 0;
	int ret = GetQueuedCompletionStatusEx(m_handle, &m_events[0], (ULONG)m_events.size(), &event_count, (DWORD)timeout, false);
	if (ret)
	{
		for (ULONG i = 0; i < event_count; i++)
		{
			OVERLAPPED_ENTRY& oe = m_events[i];
			auto object = (socket_object*)oe.lpCompletionKey;
			object->on_complete(this, oe.lpOverlapped);
		}
	}
#endif

#ifdef __linux
	int event_count = epoll_wait(m_handle, &m_events[0], (int)m_events.size(), timeout);
	for (int i = 0; i < event_count; i++)
	{
		epoll_event& ev = m_events[i];
		auto object = (socket_object*)ev.data.ptr;
        object->on_complete(this, ev.events & EPOLLIN != 0, ev.events & EPOLLOUT != 0);
	}
#endif

#ifdef __APPLE__
	timespec time_wait;
	time_wait.tv_sec = timeout / 1000;
	time_wait.tv_nsec = (timeout % 1000) * 1000000;
	int event_count = kevent(m_handle, nullptr, 0, &m_events[0], (int)m_events.size(), timeout >= 0 ? &time_wait : nullptr);
	for (int i = 0; i < event_count; i++)
	{
		struct kevent& ev = m_events[i];
		auto object = (socket_object*)ev.udata;
		assert(ev.filter == EVFILT_READ || ev.filter == EVFILT_WRITE);
        object->on_complete(this, ev.filter == EVFILT_READ, ev.filter == EVFILT_WRITE);
	}
#endif

	m_dns.update();

	auto it = m_objects.begin(), end = m_objects.end();
	while (it != end)
	{
		socket_object* object = it->second;
		if (!object->update(this))
		{
			it = m_objects.erase(it);
			delete object;
			continue;
		}
		++it;
	}
}

int socket_manager::listen(std::string& err, const char ip[], int port)
{
	int ret = false;
	socket_t fd = INVALID_SOCKET;
	sockaddr_storage addr;
	size_t addr_len = 0;
	int one = 1;
	auto* listener = new socket_listener();

	ret = make_ip_addr(&addr, &addr_len, ip, port);
	FAILED_JUMP(ret);

	fd = socket(addr.ss_family, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(fd != INVALID_SOCKET);

	set_none_block(fd);

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
	FAILED_JUMP(ret != SOCKET_ERROR);

	// macOSX require addr_len to be the real len (ipv4/ipv6)
	ret = bind(fd, (sockaddr*)&addr, (int)addr_len);
	FAILED_JUMP(ret != SOCKET_ERROR);

	ret = ::listen(fd, 16);
	FAILED_JUMP(ret != SOCKET_ERROR);

	if (watch(fd, listener, true, false) && listener->setup(fd))
	{
		int token = new_token();
		m_objects[token] = listener;
		return token;
	}

Exit0:
	get_error_string(err, get_socket_error());
	delete listener;
	if (fd != INVALID_SOCKET)
	{
		close_socket_handle(fd);
		fd = INVALID_SOCKET;
	}
	return 0;
}

int socket_manager::connect(std::string& err, const char domain[], const char service[])
{
	int token = new_token();
	socket_stream* stm = new socket_stream();
	dns_request_t* req = new dns_request_t;

	req->node = domain;
	req->service = service;

	req->dns_cb = [this, token](addrinfo* addr)
	{
		socket_object* obj = get_object(token);
		if (obj)
		{
			obj->connect(addr);
		}
		else
		{
			freeaddrinfo(addr);
		}
	};

	req->err_cb = [this, token](const char* err)
	{
		socket_object* obj = get_object(token);
		if (obj)
		{
			obj->on_dns_err(err);
		}
	};

	m_dns.request(req);
	m_objects[token] = stm;

	return token;
}

void socket_manager::set_send_cache(int token, size_t size)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_send_cache(size);
	}
}

void socket_manager::set_recv_cache(int token, size_t size)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_recv_cache(size);
	}
}

void socket_manager::send(int token, const void* data, size_t data_len)
{
	auto node = get_object(token);
	if (node)
	{
		node->send(data, data_len);
	}
}

void socket_manager::close(int token)
{
	auto node = get_object(token);
	if (node)
	{
		node->close();
	}
}

bool socket_manager::get_remote_ip(std::string& ip, int token)
{
	return false;
}

void socket_manager::set_accept_callback(int token, const std::function<void(int)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_accept_callback(cb);
	}
}

void socket_manager::set_connect_callback(int token, const std::function<void()>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_connect_callback(cb);
	}
}

void socket_manager::set_package_callback(int token, const std::function<void(char*, size_t)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_package_callback(cb);
	}
}

void socket_manager::set_error_callback(int token, const std::function<void(const char*)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_error_callback(cb);
	}
}

bool socket_manager::watch(socket_t fd, socket_object* object, bool watch_recv, bool watch_send, bool modify)
{
#ifdef _MSC_VER
	auto ret = CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)object, 0);
	if (ret != m_handle)
		return false;
#endif

#ifdef __linux
	epoll_event ev;
	ev.data.ptr = object;
	ev.events = EPOLLET;

	assert(watch_recv || watch_send);

	if (watch_recv)
	{
		ev.events |= EPOLLIN;
	}

	if (watch_send)
	{
		ev.events |= EPOLLOUT;
	}

	auto ret = epoll_ctl(m_handle, modify ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &ev);
	if (ret != 0)
    {
        char txt[128];
        get_error_string(txt, sizeof(txt), errno);
        printf("epoll_ctl: %s\n", txt);
	    return false;
    }
#endif

#ifdef __APPLE__
	struct kevent ev[2];
	struct kevent* pev = ev;

	assert(watch_recv || watch_send);

	if (watch_recv)
	{
		EV_SET(pev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, object);
		pev++;
	}

	if (watch_send)
	{
		EV_SET(pev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, object);
		pev++;
	}

	auto ret = kevent(m_handle, ev, (int)(pev - ev), nullptr, 0, nullptr);
	if (ret != 0)
		return false;
#endif

	return true;
}

int socket_manager::accept_stream(socket_t fd)
{
	auto* stm = new socket_stream();
	if (watch(fd, stm, true, true) && stm->accept_socket(fd))
	{
		auto token = new_token();
		m_objects[token] = stm;
		return token;
	}
	delete stm;
	return 0;
}

std::shared_ptr<socket_mgr> create_socket_mgr(int max_fd)
{
	std::shared_ptr<socket_manager> mgr = std::make_shared<socket_manager>();
	if (mgr->setup(max_fd))
	{
		return mgr;
	}
	return nullptr;
}
