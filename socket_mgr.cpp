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
#include "tools.h"
#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"

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
			auto stream = (socket_stream*)oe.lpCompletionKey;
			stream->OnComplete(oe.lpOverlapped, oe.dwNumberOfBytesTransferred);
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
			stream->on_recvable();
		}
		if (ev.events & EPOLLOUT)
		{
			stream->on_sendable();
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
			stream->on_recvable();
		}
		else if (ev.filter == EVFILT_WRITE)
		{
			stream->on_sendable();
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
	socket_listener* listener = new socket_listener();
	if (!listener->listen(err, ip, port))
	{
		delete listener;
		return 0;
	}
	int64_t token = new_token();
	m_objects[token] = listener;
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

int64_t socket_manager::new_stream(socket_t fd)
{
	int64_t token = new_token();
	socket_stream* stm = new socket_stream(fd);

#ifdef _MSC_VER
	HANDLE handle = CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)stm, 0);
	if (handle != m_handle || !stm->queue_recv())
	{
		delete stm;
		return 0;
	}
#endif

#ifdef __linux
	epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.ptr = stm;
	int nRetCode = epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &ev);
	if (nRetCode == -1)
	{
		delete stm;
		return 0;
	}
#endif

#ifdef __APPLE__
	struct kevent ev[2];
	EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, stm);
	EV_SET(&ev[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, stm);
	int nRetCode = kevent(m_handle, ev, _countof(ev), nullptr, 0, nullptr);
	if (nRetCode == -1)
	{
		delete stm;
		return 0;
	}
#endif

	m_objects[token] = stm;

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
