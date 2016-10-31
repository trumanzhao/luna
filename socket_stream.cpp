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
#include "socket_stream.h"

socket_stream::~socket_stream()
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

void socket_stream::accept_socket(socket_t fd)
{
	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	getpeername(fd, (struct sockaddr*)&addr, &len);
	get_ip_string(m_ip, sizeof(m_ip), &addr, sizeof(addr));
	m_socket = fd;
	m_connected = true;
}

void socket_stream::on_dns_err(const char* err)
{
	if (!m_closed)
	{
		assert(!m_connected);
		call_error(err);
		m_closed = true;
	}
}

bool socket_stream::update(socket_manager* mgr)
{
	if (m_connected)
	{
		return !m_closed;
	}

	if (m_closed)
	{
		return false;
	}

	// wait for dns resolver
	if (m_addr == nullptr)
		return true;

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
		get_ip_string(m_ip, sizeof(m_ip), m_next->ai_addr, m_next->ai_addrlen);

		while (m_socket != INVALID_SOCKET)
		{
			ret = ::connect(m_socket, m_next->ai_addr, (int)m_next->ai_addrlen);
			if (ret != SOCKET_ERROR)
			{
				m_connected = true;
				freeaddrinfo(m_addr);
				m_addr = nullptr;
				m_next = nullptr;
				m_connect_cb();
				return true;
			}

			int err = get_socket_error();
#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
			{
				if (!mgr->watch(m_socket, this, false, true) || !wsa_send_empty(m_socket, m_send_ovl))
				{
					m_closed = true;
					m_error_cb("connecting_watch_failed");
					return false;
				}
				break;
			}
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
		m_closed = true;
		m_error_cb("socket_error");
		return false;
	}

	return true;
}

void socket_stream::send(const void* data, size_t data_len)
{
	if (m_closed)
		return;

	BYTE  header[MAX_HEADER_LEN];
	size_t header_len = encode_u64(header, sizeof(header), data_len);
	stream_send((char*)header, header_len);
	stream_send((char*)data, data_len);
}

void socket_stream::stream_send(const char* data, size_t data_len)
{
	if (m_closed)
		return;

	if (m_send_buffer.HasData())
	{
		if (!m_send_buffer.PushData(data, data_len))
		{
			m_closed = true;
			call_error("send_cache_full");
		}
		return;
	}

	while (data_len > 0)
	{
		size_t try_len = data_len < MAX_SIZE_PER_SEND ? data_len : MAX_SIZE_PER_SEND;
		int send_len = ::send(m_socket, data, (int)try_len, 0);
		if (send_len == SOCKET_ERROR)
		{
			int err = get_socket_error();

#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
			{
				if (!m_send_buffer.PushData(data, data_len))
				{
					m_closed = true;
					call_error("send_cache_full");
					return;
				}

				return;
			}
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINTR)
				continue;

			if (err == EAGAIN)
			{
				if (!m_send_buffer.PushData(data, data_len))
				{
					call_error("send_cache_full");
				}
				return;
			}
#endif

			call_error(err);
			return;
		}

		if (send_len == 0)
		{
			call_error("connection_lost");
			return;
		}

		data += send_len;
		data_len -= send_len;
	}
}

#ifdef _MSC_VER
void socket_stream::on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl)
{
	if (m_closed)
		return;

	if (m_connected)
	{
		if (ovl == &m_recv_ovl)
		{
			do_recv();
		}
		else
		{
			do_send();
		}
		return;
	}

	int err = 0;
	socklen_t sock_len = sizeof(err);
	auto ret = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&err, &sock_len);
	if (ret == 0 && err == 0)
	{
		freeaddrinfo(m_addr);
		m_addr = nullptr;
		m_next = nullptr;
		m_connected = true;
		m_connect_cb();
		return;
	}

	// socket连接失败,还可以继续dns解析的下一个地址继续尝试
	close_socket_handle(m_socket);
	m_socket = INVALID_SOCKET;
	if (m_next == nullptr)
	{
		m_error_cb("connect_failed");
		m_closed = true;
	}
}
#endif

void socket_stream::do_send()
{
	while (!m_closed)
	{
		size_t data_len = 0;
		auto* data = m_send_buffer.GetData(&data_len);
		if (data_len == 0)
			break;

		size_t try_len = data_len < MAX_SIZE_PER_SEND ? data_len : MAX_SIZE_PER_SEND;
		int send_len = ::send(m_socket, (char*)data, (int)try_len, 0);
		if (send_len == SOCKET_ERROR)
		{
			int err = get_socket_error();

#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
			{
				if (!wsa_send_empty(m_socket, m_send_ovl))
				{
					m_closed = true;
					call_error("wsa_send_failed");
					return;
				}
				break;
			}
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINTR)
				continue;

			if (err == EAGAIN)
				break;
#endif

			m_closed = true;
			call_error(err);
			return;
		}

		if (send_len == 0)
		{
			m_closed = true;
			call_error("connection_lost");
			return;
		}

		m_send_buffer.PopData((size_t)send_len);
	}

	m_send_buffer.MoveDataToFront();
}

void socket_stream::do_recv()
{
	while (!m_closed)
	{
		size_t space_len = 0;
		auto* space = m_recv_buffer.GetSpace(&space_len);
		if (space_len == 0)
		{
			m_closed = true;
			call_error("package_too_large");
			return;
		}

		int recv_len = recv(m_socket, (char*)space, (int)space_len, 0);
		if (recv_len == -1)
		{
			int err = get_socket_error();

#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
			{
				if (!wsa_recv_empty(m_socket, m_recv_ovl))
				{
					m_closed = true;
					call_error("wsa_recv_failed");
					return;
				}
				break;
			}
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINTR)
				continue;

			if (err == EAGAIN)
				break;
#endif

			m_closed = true;
			call_error(err);
			return;
		}

		if (recv_len == 0)
		{
			m_closed = true;
			call_error("connection_lost");
			return;
		}

		m_recv_buffer.PopSpace(recv_len);
		dispatch_package();
	}
}

void socket_stream::dispatch_package()
{
	while (!m_closed)
	{
		size_t data_len = 0;
		auto* data = m_recv_buffer.GetData(&data_len);

		uint64_t package_size = 0;
		size_t header_len = decode_u64(&package_size, data, data_len);
		if (header_len == 0)
			break;

		// 数据包还没有收完整
		if (data_len < header_len + package_size)
			break;

		m_package_cb(data + header_len, (size_t)package_size);

		m_recv_buffer.PopData(header_len + (size_t)package_size);
	}

	m_recv_buffer.MoveDataToFront();
}

void socket_stream::call_error(int err)
{
	char txt[MAX_ERROR_TXT];
	get_error_string(txt, _countof(txt), err);
	call_error(txt);
}

void socket_stream::call_error(const char err[])
{
	if (!m_closed)
	{
		m_error_cb(err);
		m_closed = true;
	}
}
