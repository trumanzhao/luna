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
#include <assert.h>
#include "tools.h"
#include "var_int.h"
#include "socket_mgr.h"
#include "socket_object.h"

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

socket_listener::~socket_listener()
{
	if (m_socket != INVALID_SOCKET)
	{
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
	}
}

bool socket_listener::listen(std::string& err, const char ip[], int port)
{
	bool result = false;
	int ret = false;
	sockaddr_storage addr;
	size_t addr_len = 0;
	int one = 1;

	ret = make_ip_addr(&addr, &addr_len, ip, port);
	FAILED_JUMP(ret);

	m_socket = socket(addr.ss_family, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(m_socket != INVALID_SOCKET);

	set_none_block(m_socket);

	ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
	FAILED_JUMP(ret != SOCKET_ERROR);

	// macOSX require addr_len to be the real len (ipv4/ipv6)
	ret = bind(m_socket, (sockaddr*)&addr, (int)addr_len);
	FAILED_JUMP(ret != SOCKET_ERROR);

	ret = ::listen(m_socket, 16);
	FAILED_JUMP(ret != SOCKET_ERROR);

	result = true;
Exit0:
	if (!result)
	{
		get_error_string(err, get_socket_error());
	}
	return result;
}

bool socket_listener::update(socket_manager* mgr)
{
	while (!m_closed)
	{
		socket_t fd = accept(m_socket, nullptr, nullptr);
		if (fd == INVALID_SOCKET)
			break;

		set_none_block(fd);

		int64_t token = mgr->new_stream(fd);
		if (token != 0)
		{
			m_accept_cb(token);
		}
		else
		{
			close_socket_handle(fd);
			m_error_cb("new_stream_failed");
		}
	}
	return !m_closed;
}

socket_stream::socket_stream(socket_t fd)
{
	sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
	getpeername(fd, (struct sockaddr*)&addr, &len);
	get_ip_string(m_ip, sizeof(m_ip), addr);
	m_socket = fd;
}

socket_stream::~socket_stream()
{
	if (m_socket != INVALID_SOCKET)
	{
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
	}
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
			call_error("send_cache_full");
		}
		return;
	}

	while (data_len > 0)
	{
		size_t try_len = data_len < MAX_SIZE_PER_SEND ? data_len : MAX_SIZE_PER_SEND;
		int send_len = ::send(m_socket, data, (int)try_len, 0);
		if (send_len == -1)
		{
			int err = get_socket_error();

#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
			{
				if (!m_send_buffer.PushData(data, data_len))
				{
					call_error("send_cache_full");
					return;
				}
				queue_send();
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
static char s_zero = 0;
bool socket_stream::queue_send()
{
	DWORD bytes = 0;
	WSABUF ws_buf = {0, &s_zero};

	memset(&m_send_ovl, 0, sizeof(m_send_ovl));
	int ret = WSASend(m_socket, &ws_buf, 1, &bytes, 0, &m_send_ovl, nullptr);
	if (ret == SOCKET_ERROR)
	{
		int nErr = get_socket_error();
		if (nErr == WSA_IO_PENDING)
		{
			return true;
		}
	}
	return false;
}

bool socket_stream::queue_recv()
{
	DWORD bytes = 0;
	DWORD flags = 0;
	WSABUF ws_buf = {0, &s_zero};

	memset(&m_recv_ovl, 0, sizeof(m_recv_ovl));
	int nRetCode = WSARecv(m_socket, &ws_buf, 1, &bytes, &flags, &m_recv_ovl, nullptr);
	if (nRetCode == SOCKET_ERROR)
	{
		int nErr = get_socket_error();
		if (nErr == WSA_IO_PENDING)
		{
			return true;
		}
	}
	return false;
}

void socket_stream::OnComplete(WSAOVERLAPPED* pOVL, DWORD)
{
	if (pOVL == &m_recv_ovl)
	{
		on_recvable();
	}
	else
	{
		assert(pOVL == &m_send_ovl);
		on_sendable();
	}
}
#endif

void socket_stream::on_sendable()
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
				break;
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINTR)
				continue;

			if (nEerrrr == EAGAIN)
				break;
#endif

			call_error(err);
			return;
		}

		if (send_len == 0)
		{
			call_error("connection_lost");
			return;
		}

		m_send_buffer.PopData((size_t)send_len);
	}

	m_send_buffer.MoveDataToFront();
}

void socket_stream::on_recvable()
{
	while (!m_closed)
	{
		size_t space_len = 0;
		auto* space = m_recv_buffer.GetSpace(&space_len);

		if (space_len == 0)
		{
			call_error("package_too_long");
			return;
		}

		int recv_len = recv(m_socket, (char*)space, (int)space_len, 0);
		if (recv_len == -1)
		{
			int err = get_socket_error();

#ifdef _MSC_VER
			if (err == WSAEWOULDBLOCK)
				break;
#endif

#if defined(__linux) || defined(__APPLE__)
			if (err == EINTR)
				continue;

			if (err == EAGAIN)
				break;
#endif

			call_error(err);
			return;
		}

		if (recv_len == 0)
		{
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
