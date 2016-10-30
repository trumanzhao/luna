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
#include <assert.h>
#include "tools.h"
#include "var_int.h"
#include "socket_mgr.h"
#include "socket_connector.h"

socket_connector::~socket_connector()
{
	if (m_socket != INVALID_SOCKET)
	{
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
	}

	if (m_addr != nullptr)
	{
		freeaddrinfo(m_addr);
		m_addr = nullptr;
	}
}

bool socket_connector::update(socket_manager* mgr)
{
	if (m_closed)
	{
		return false;
	}

	if (m_timeout >= 0)
	{
		int64_t now = get_time_ms();
		if (now > m_start_time + m_timeout)
		{
			m_error_cb("request_timeout");
			return false;
		}
	}

	// wait for dns resolver
	if (m_addr == nullptr)
		return true;

	int ret = 0;
	auto addr = m_next;
	while (m_socket == INVALID_SOCKET && addr != nullptr)
	{
		if (addr->ai_family != AF_INET && addr->ai_family != AF_INET6)
		{
			addr = addr->ai_next;
			continue;
		}

		m_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (m_socket == INVALID_SOCKET)
		{
			addr = addr->ai_next;
			continue;
		}

		set_none_block(m_socket);

		while (m_socket != INVALID_SOCKET)
		{
			ret = ::connect(m_socket, addr->ai_addr, (int)addr->ai_addrlen);
			if (ret != SOCKET_ERROR)
				break;

			int err = get_socket_error();
#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
				break;
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINPROGRESS)
				break;

			if (err == EINTR)
				continue;
#endif

			close_socket_handle(m_socket);
			m_socket = INVALID_SOCKET;
		}

		addr = addr->ai_next;
	}
	m_next = addr;

	if (m_socket == INVALID_SOCKET)
	{
		m_error_cb("socket_error");
		return false;
	}

	if (!check_can_write(m_socket, 0))
		return true;

	int err = 0;
	socklen_t sock_len = sizeof(err);
	ret = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&err, &sock_len);
	if (ret == 0 && err == 0)
	{
		int64_t token = mgr->new_stream(m_socket);
		if (token != 0)
		{
			m_socket = INVALID_SOCKET;
			m_connect_cb(token);
		}
		else
		{
			m_error_cb("new_stream_failed");
		}
		return false;
	}
	else
	{
		// socket连接失败,还可以继续dns解析的下一个地址(下一次调用的时候)
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
		if (m_next == nullptr)
		{
			m_error_cb("connect_failed");
			return false;
		}
	}
	return true;
}

void socket_connector::on_dns_err(const char* err)
{
	if (!m_closed)
	{
		m_error_cb(err);
		m_closed = true;
	}
}

void socket_connector::on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl)
{
}
