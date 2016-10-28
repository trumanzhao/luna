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

bool socket_stream::update(socket_manager*)
{
#ifdef _MSC_VER
	return !(m_closed && m_recv_complete && m_send_complete);
#endif

#if defined(__linux) || defined(__APPLE__)
	return !m_closed;
#endif
}

void socket_stream::send(const void* data, size_t data_len)
{
	if (m_closed)
		return;

	BYTE  header[MAX_HEADER_LEN];
	size_t header_len = encode_u64(header, sizeof(header), data_len);
	StreamSend(header, header_len);
	StreamSend(data, data_len);
}

void socket_stream::StreamSend(const void* pvData, size_t uDataLen)
{
	if (m_closed)
		return;

#ifdef _MSC_VER
	if (!m_send_complete)
	{
		if (!m_SendBuffer.PushData(pvData, uDataLen))
		{
			call_error("send_cache_full");
		}
		return;
	}
#endif

#if defined(__linux) || defined(__APPLE__)
	if (!m_bWriteAble)
	{
		if (!m_SendBuffer.PushData(pvData, uDataLen))
		{
			call_error("send_cache_full");
		}
		return;
	}
#endif

	const char*	pbyData = (char*)pvData;
	while (uDataLen > 0)
	{
		size_t uTryLen = uDataLen < MAX_SIZE_PER_SEND ? uDataLen : MAX_SIZE_PER_SEND;
		int nSend = ::send(m_socket, pbyData, (int)uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = get_socket_error();

#ifdef _MSC_VER
			if (nErr == WSAEWOULDBLOCK)
			{
				if (!m_SendBuffer.PushData(pbyData, uDataLen))
				{
					call_error("send_cache_full");
					return;
				}
				AsyncSend();
				return;
			}
#endif

#if defined(__linux) || defined(__APPLE__)
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
			{
				m_bWriteAble = false;

				if (!m_SendBuffer.PushData(pbyData, uDataLen))
				{
					call_error("send_cache_full");
				}
				return;
			}
#endif

			call_error(nErr);
			return;
		}

		if (nSend == 0)
		{
			call_error("connection_lost");
			return;
		}

		pbyData += nSend;
		uDataLen -= nSend;
	}
}

#ifdef _MSC_VER
void socket_stream::AsyncSend()
{
	int nRetCode = 0;
	DWORD dwSendBytes = 0;
	BYTE* pbyData = nullptr;
	size_t uDataLen = 0;
	WSABUF wsBuf;

	assert(m_send_complete);

	pbyData = m_SendBuffer.GetData(&uDataLen);
	if (uDataLen == 0)
		return;

	wsBuf.len = (u_long)uDataLen;
	wsBuf.buf = (CHAR*)pbyData;

	memset(&m_wsSendOVL, 0, sizeof(m_wsSendOVL));

	m_send_complete = false;

	nRetCode = WSASend(m_socket, &wsBuf, 1, &dwSendBytes, 0, &m_wsSendOVL, nullptr);
	if (nRetCode == SOCKET_ERROR)
	{
		int nErr = get_socket_error();
		if (nErr != WSA_IO_PENDING)
		{
			m_send_complete = true;
			call_error(nErr);
		}
	}
}

void socket_stream::AsyncRecv()
{
	int nRetCode = 0;
	DWORD dwRecvBytes = 0;
	DWORD dwFlags = 0;
	BYTE* pbySpace = nullptr;
	size_t uSpaceLen = 0;
	WSABUF wsBuf;

	assert(m_recv_complete);

	pbySpace = m_RecvBuffer.GetSpace(&uSpaceLen);
	if (uSpaceLen == 0)
	{
		call_error("recv_package_too_large");
		return;
	}

	wsBuf.len = (u_long)uSpaceLen;
	wsBuf.buf = (CHAR*)pbySpace;

	memset(&m_wsRecvOVL, 0, sizeof(m_wsRecvOVL));

	m_recv_complete = false;

	nRetCode = WSARecv(m_socket, &wsBuf, 1, &dwRecvBytes, &dwFlags, &m_wsRecvOVL, nullptr);
	if (nRetCode == SOCKET_ERROR)
	{
		int nErr = get_socket_error();
		if (nErr != WSA_IO_PENDING)
		{
			m_recv_complete = true;
			call_error(nErr);
		}
	}
}

void socket_stream::OnComplete(WSAOVERLAPPED* pOVL, DWORD dwLen)
{
	if (pOVL == &m_wsRecvOVL)
	{
		OnRecvComplete((size_t)dwLen);
		return;
	}

	assert(pOVL == &m_wsSendOVL);
	OnSendComplete((size_t)dwLen);
}

void socket_stream::OnRecvComplete(size_t uLen)
{
	m_recv_complete = true;
	if (m_closed)
		return;

	if (uLen == 0)
	{
		call_error("connection_lost");
		return;
	}

	m_RecvBuffer.PopSpace(uLen);

	DispatchPackage();

	AsyncRecv();
}

void socket_stream::OnSendComplete(size_t uLen)
{
	m_send_complete = true;
	if (m_closed)
		return;

	if (uLen == 0)
	{
		call_error("connection_lost");
		return;
	}

	m_SendBuffer.PopData(uLen);
	m_SendBuffer.MoveDataToFront();

	AsyncSend();
}
#endif

#if defined(__linux) || defined(__APPLE__)
void socket_stream::OnSendAble()
{
	while (!m_closed)
	{
		size_t uDataLen = 0;
		BYTE* pbyData = m_SendBuffer.GetData(&uDataLen);

		if (uDataLen == 0)
		{
			m_bWriteAble = true;
			break;
		}

		size_t uTryLen = uDataLen < MAX_SIZE_PER_SEND ? uDataLen : MAX_SIZE_PER_SEND;
		int nSend = ::send(m_socket, pbyData, uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = get_socket_error();
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
				break;

			call_error(nErr);
			return;
		}

		if (nSend == 0)
		{
			call_error("Send 0, connection lost !");
			return;
		}

		m_SendBuffer.PopData((size_t)nSend);
	}

	m_SendBuffer.MoveDataToFront();
}

void socket_stream::OnRecvAble()
{
	while (!m_closed)
	{
		size_t uSpaceSize = 0;
		BYTE* pbySpace = m_RecvBuffer.GetSpace(&uSpaceSize);

		if (uSpaceSize == 0)
		{
			call_error("Recv package too large !");
			return;
		}

		int nRecv = recv(m_socket, pbySpace, uSpaceSize, 0);
		if (nRecv == -1)
		{
			int nErr = get_socket_error();
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
				return;

			call_error(nErr);
			return;
		}

		if (nRecv == 0)
		{
			call_error("Recv 0, connection lost !");
			return;
		}

		m_RecvBuffer.PopSpace(nRecv);

		DispatchPackage();
	}
}
#endif

void socket_stream::DispatchPackage()
{
	while (!m_closed)
	{
		BYTE* pbyData = nullptr;
		size_t uDataLen = 0;

		pbyData = m_RecvBuffer.GetData(&uDataLen);

		uint64_t uPackageSize = 0;
		size_t uHeaderLen = decode_u64(&uPackageSize, pbyData, uDataLen);
		if (uHeaderLen == 0)
			break;

		// 数据包还没有收完整
		if (uDataLen < uHeaderLen + uPackageSize)
			break;

		m_package_cb(pbyData + uHeaderLen, (size_t)uPackageSize);

		m_RecvBuffer.PopData(uHeaderLen + (size_t)uPackageSize);
	}

	m_RecvBuffer.MoveDataToFront();
}

void socket_stream::call_error(int err)
{
	char txt[128];
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
