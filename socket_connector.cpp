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
#include <string.h>
#include "tools.h"
#include "socket_mgr.h"
#include "socket_connector.h"


xconnector_t::xconnector_t(XSocketManager* mgr, const char node[], const char service[])
{
	m_mgr = mgr;
	m_node = node;
	m_service = service;
	m_start_time = get_time_ms();
	m_thread = std::thread(&xconnector_t::work, this);
}

xconnector_t::~xconnector_t()
{
	m_thread.join();

	if (m_socket != INVALID_SOCKET)
	{
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
	}

	if (m_addr)
	{
		freeaddrinfo(m_addr);
		m_addr = nullptr;
	}
}

// 用户可以在任何时刻close,但是要m_dns_resolved完成后才会真的删除对象
bool xconnector_t::update()
{
	if (!m_dns_resolved)
		return true;

	if (!m_user_closed)
	{
		while (!m_callbacked)
		{
			do_connect();

			if (m_socket != INVALID_SOCKET)
				break;
		}
	}

	return !m_user_closed;
}

void xconnector_t::work()
{
	addrinfo hints;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo(m_node.c_str(), m_service.c_str(), &hints, &m_addr);
	if (ret != 0)
	{
#if defined(__linux) || defined(__APPLE__)
		m_dns_error = gai_strerror(ret);
#endif

#if _MSC_VER
		m_dns_error = gai_strerrorA(ret);
#endif
	}
	m_next = m_addr;
	m_dns_resolved = true;
}

void xconnector_t::do_connect()
{
	if (m_addr == nullptr)
	{
		m_error_cb(m_dns_error.c_str());
		m_callbacked = true;
		return;
	}

	int ret = 0;

	while (m_socket == INVALID_SOCKET && m_next != nullptr)
	{
		if (m_next->ai_family != AF_INET && m_next->ai_family != AF_INET6)
		{
			m_next = m_next->ai_next;
			continue;
		}

		m_socket = socket(m_next->ai_family, m_next->ai_socktype, m_next->ai_protocol);
		if (m_socket == INVALID_SOCKET)
		{
			m_next = m_next->ai_next;
			continue;
		}

		set_none_block(m_socket);

		while (m_socket != INVALID_SOCKET)
		{
			ret = connect(m_socket, m_next->ai_addr, (int)m_next->ai_addrlen);
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

		m_next = m_next->ai_next;
	}

	if (m_socket == INVALID_SOCKET)
	{
		m_error_cb("socket_error");
		m_callbacked = true;
		return;
	}

	if (!check_can_write(m_socket, 0))
	{
		int64_t now = get_time_ms();
		if (m_timeout >= 0 && now > m_start_time + m_timeout)
		{
			close_socket_handle(m_socket);
			m_socket = INVALID_SOCKET;
			m_error_cb("request_timeout");
			m_callbacked = true;
		}
		return;
	}

	int err = 0;
	socklen_t sock_len = sizeof(err);
	ret = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&err, &sock_len);
	if (ret == 0 && err == 0)
	{
		char ip[INET6_ADDRSTRLEN];
		sockaddr_storage addr;
		socklen_t len = sizeof(addr);
		memset(&addr, 0, sizeof(addr));
		getpeername(m_socket, (struct sockaddr*)&addr, &len);
		get_ip_string(ip, sizeof(ip), addr);
		auto stream = m_mgr->CreateStreamSocket(m_socket, m_recv_buffer_size, m_send_buffer_size, ip);
		if (stream)
		{
			m_socket = INVALID_SOCKET;
			m_connect_cb(stream);
		}
		else
		{
			close_socket_handle(m_socket);
			m_socket = INVALID_SOCKET;
			m_error_cb("create_stream_failed");
		}
		m_callbacked = true;
	}
	else
	{
		// socket连接失败,继续dns解析的下一个地址(下一次调用的时候)
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
		if (m_next == nullptr)
		{
			m_error_cb("connect_failed");
			m_callbacked = true;
		}
	}
}
