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
#include "socket_connector.h"

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
		auto stream = (socket_stream*)ev.data.ptr;
		if (ev.events & EPOLLIN)
		{
			stream->do_recv();
		}
		if (ev.events & EPOLLOUT)
		{
			stream->do_send();
		}
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
		auto stream = (socket_stream*)ev.udata;
		assert(ev.filter == EVFILT_READ || ev.filter == EVFILT_WRITE);
		if (ev.filter == EVFILT_READ)
		{
			stream->do_recv();
		}
		else if (ev.filter == EVFILT_WRITE)
		{
			stream->do_send();
		}
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

int64_t socket_manager::listen(std::string& err, const char ip[], int port)
{
	int ret = false;
	socket_t fd = INVALID_SOCKET;
	sockaddr_storage addr;
	size_t addr_len = 0;
	int one = 1;	
	auto* listener = new socket_listener();
	int64_t token = 0;

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

	token = register_object(fd, listener, false);
	FAILED_JUMP(token != 0);

	if (!listener->setup(fd))
	{
		m_objects.erase(token);
		token = 0;
	}

Exit0:
	if (token == 0)
	{
		get_error_string(err, get_socket_error());
		delete listener;
		if (fd != INVALID_SOCKET)
		{
			close_socket_handle(fd);
			fd = INVALID_SOCKET;
		}
	}
	return token;
}

int64_t socket_manager::connect(std::string& err, const char domain[], const char service[], int timeout)
{
	int64_t token = new_token();
	socket_connector* connector = new socket_connector();
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
	m_objects[token] = connector;

	return token;
}

void socket_manager::set_send_cache(int64_t token, size_t size)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_send_cache(size);
	}
}

void socket_manager::set_recv_cache(int64_t token, size_t size)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_recv_cache(size);
	}
}

void socket_manager::send(int64_t token, const void* data, size_t data_len)
{
	auto node = get_object(token);
	if (node)
	{
		node->send(data, data_len);
	}
}

void socket_manager::close(int64_t token)
{
	auto node = get_object(token);
	if (node)
	{
		node->close();
	}
}

bool socket_manager::get_remote_ip(std::string& ip, int64_t token)
{
	return false;
}

void socket_manager::set_listen_callback(int64_t token, const std::function<void(int64_t)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_listen_callback(cb);
	}
}

void socket_manager::set_connect_callback(int64_t token, const std::function<void(int64_t)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_connect_callback(cb);
	}
}

void socket_manager::set_package_callback(int64_t token, const std::function<void(BYTE*, size_t)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_package_callback(cb);
	}
}

void socket_manager::set_error_callback(int64_t token, const std::function<void(const char*)>& cb)
{
	auto node = get_object(token);
	if (node)
	{
		node->set_error_callback(cb);
	}
}

int64_t socket_manager::register_object(socket_t fd, socket_object* object, bool with_write)
{
#ifdef _MSC_VER
	auto ret = CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)object, 0);
	if (ret != m_handle)
		return 0;
#endif

#ifdef __linux
	epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = object;

	if (with_write)
	{
		ev.events |= EPOLLOUT;
	}

	auto ret = epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &ev);
	if (ret != 0)
		return 0;
#endif

#ifdef __APPLE__
	struct kevent ev[2];
	EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, object);
	EV_SET(&ev[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, object);
	auto ret = kevent(m_handle, ev, with_write ? 2 : 1, nullptr, 0, nullptr);
	if (ret != 0)
		return 0;
#endif

	auto token = new_token();
	m_objects[token] = object;
	return token;
}

int64_t socket_manager::new_stream(socket_t fd)
{
	auto* stm = new socket_stream();
	auto token = register_object(fd, stm, true);
	if (token == 0)
	{
		delete stm;
		return 0;
	}

	if (!stm->setup(fd))
	{
		m_objects.erase(token);
		delete stm;
		return 0;
	}

	return token;
}

socket_mgr* create_socket_mgr(int max_fd)
{
	auto mgr = new socket_manager();
	if (mgr->setup(max_fd))
	{
		return mgr;
	}
	return nullptr;
}
