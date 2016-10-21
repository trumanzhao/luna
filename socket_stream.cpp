#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#include <assert.h>
#include "tools.h"
#include "var_int.h"
#include "socket_mgr.h"
#include "socket_stream.h"

XSocketStream::~XSocketStream()
{
	if (m_nSocket != INVALID_SOCKET)
	{
		CloseSocketHandle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
}

void XSocketStream::Activate()
{
	if (m_bErrored && (!m_bErrorNotified) && (!m_bUserClosed))
	{
		m_ErrorCallback();
		m_bErrorNotified = true;
	}
}

void XSocketStream::Send(const void* pvData, size_t uDataLen)
{
	if (m_bUserClosed || m_bErrored)
		return;

	BYTE  byHeader[MAX_HEADER_LEN];
	size_t uHeaderLen = encode_u64(byHeader, sizeof(byHeader), uDataLen);
	StreamSend(byHeader, uHeaderLen);
	StreamSend(pvData, uDataLen);
}

void XSocketStream::Close()
{	
#if defined(__linux) || defined(__APPLE__)
	// Windows下要等IO完成,不能立即关闭?
	if (m_nSocket != INVALID_SOCKET)
	{
		CloseSocketHandle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
#endif
	m_bUserClosed = true;
}

void XSocketStream::StreamSend(const void* pvData, size_t uDataLen)
{
	if (m_bErrored)
		return;

#ifdef _MSC_VER
	if (!m_bSendComplete)
	{
		if (!m_SendBuffer.PushData(pvData, uDataLen))
		{
			SetError("send_cache_full");
		}
		return;
	}
#endif

#if defined(__linux) || defined(__APPLE__)
	if (!m_bWriteAble)
	{
		if (!m_SendBuffer.PushData(pvData, uDataLen))
		{
			SetError("send_cache_full");
		}
		return;
	}
#endif

	const char*	pbyData = (char*)pvData;
	while (uDataLen > 0)
	{
		size_t uTryLen = uDataLen < MAX_SIZE_PER_SEND ? uDataLen : MAX_SIZE_PER_SEND;
		int nSend = send(m_nSocket, pbyData, (int)uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = GetSocketError();

#ifdef _MSC_VER
			if (nErr == WSAEWOULDBLOCK)
			{
				if (!m_SendBuffer.PushData(pbyData, uDataLen))
				{
					SetError("send_cache_full");
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
					SetError("send_cache_full");
				}
				return;
			}
#endif

			SetError(nErr);
			return;
		}

		if (nSend == 0)
		{
			SetError("connection_lost");
			return;
		}

		pbyData += nSend;
		uDataLen -= nSend;
	}
}

#ifdef _MSC_VER
void XSocketStream::AsyncSend()
{
	int nRetCode = 0;
	DWORD dwSendBytes = 0;
	BYTE* pbyData = nullptr;
	size_t uDataLen = 0;
	WSABUF wsBuf;

	assert(m_bSendComplete);

	pbyData = m_SendBuffer.GetData(&uDataLen);
	if (uDataLen == 0)
		return;
	
	wsBuf.len = (u_long)uDataLen;
	wsBuf.buf = (CHAR*)pbyData;

	memset(&m_wsSendOVL, 0, sizeof(m_wsSendOVL));

	m_bSendComplete = false;

	nRetCode = WSASend(m_nSocket, &wsBuf, 1, &dwSendBytes, 0, &m_wsSendOVL, nullptr);
	if (nRetCode == SOCKET_ERROR)
	{
		int nErr = GetSocketError();
		if (nErr != WSA_IO_PENDING)
		{
			m_bSendComplete = true;
			SetError(nErr);
			return;
		}
	}
}

void XSocketStream::AsyncRecv()
{
	int nRetCode = 0;
	DWORD dwRecvBytes = 0;
	DWORD dwFlags = 0;
	BYTE* pbySpace = nullptr;
	size_t uSpaceLen = 0;
	WSABUF wsBuf;

	assert(m_bRecvComplete);

	pbySpace = m_RecvBuffer.GetSpace(&uSpaceLen);
	if (uSpaceLen == 0)
	{
		SetError("recv_package_too_large");
		return;
	}

	wsBuf.len = (u_long)uSpaceLen;
	wsBuf.buf = (CHAR*)pbySpace;

	memset(&m_wsRecvOVL, 0, sizeof(m_wsRecvOVL));

	m_bRecvComplete = false;

	nRetCode = WSARecv(m_nSocket, &wsBuf, 1, &dwRecvBytes, &dwFlags, &m_wsRecvOVL, nullptr);
	if (nRetCode == SOCKET_ERROR)
	{
		int nErr = GetSocketError();
		if (nErr != WSA_IO_PENDING)
		{
			m_bRecvComplete = true;
			SetError(nErr);
			return;
		}
	}
}

void XSocketStream::OnComplete(WSAOVERLAPPED* pOVL, DWORD dwLen)
{
	if (pOVL == &m_wsRecvOVL)
	{
		OnRecvComplete((size_t)dwLen);
		return;
	}

	assert(pOVL == &m_wsSendOVL);
	OnSendComplete((size_t)dwLen);
}

void XSocketStream::OnRecvComplete(size_t uLen)
{
	assert(!m_bRecvComplete);
	m_bRecvComplete = true;

	if (m_bErrored || m_bUserClosed)
		return;

	if (uLen == 0)
	{
		SetError("connection_lost");
		return;
	}

	m_RecvBuffer.PopSpace(uLen);

	DispatchPackage();

	AsyncRecv();
}

void XSocketStream::OnSendComplete(size_t uLen)
{
	assert(!m_bSendComplete);
	m_bSendComplete = true;

	if (m_bErrored || m_bUserClosed)
		return;

	if (uLen == 0)
	{
		SetError("connection_lost");
		return;
	}

	m_SendBuffer.PopData(uLen);
	m_SendBuffer.MoveDataToFront();

	AsyncSend();
}
#endif

#if defined(__linux) || defined(__APPLE__)
void XSocketStream::OnSendAble()
{
	if (m_bErrored || m_bUserClosed)
		return;

	while (true)
	{
		size_t uDataLen = 0;
		BYTE* pbyData = m_SendBuffer.GetData(&uDataLen);

		if (uDataLen == 0)
		{
			m_bWriteAble = true;
			break;
		}

		size_t uTryLen = uDataLen < MAX_SIZE_PER_SEND ? uDataLen : MAX_SIZE_PER_SEND;
		int nSend = send(m_nSocket, pbyData, uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = GetSocketError();
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
				break;

			SetError(nErr);
			return;
		}

		if (nSend == 0)
		{
			SetError("Send 0, connection lost !");
			return;
		}

		m_SendBuffer.PopData((size_t)nSend);
	}

	m_SendBuffer.MoveDataToFront();
}

void XSocketStream::OnRecvAble()
{
	if (m_bErrored || m_bUserClosed)
		return;

	while (!m_bUserClosed)
	{
		size_t uSpaceSize = 0;
		BYTE* pbySpace = m_RecvBuffer.GetSpace(&uSpaceSize);

		if (uSpaceSize == 0)
		{
			SetError("Recv package too large !");
			return;
		}

		int nRecv = recv(m_nSocket, pbySpace, uSpaceSize, 0);
		if (nRecv == -1)
		{
			int nErr = GetSocketError();
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
				return;

			SetError(nErr);
			return;
		}

		if (nRecv == 0)
		{
			SetError("Recv 0, connection lost !");
			return;
		}

		m_RecvBuffer.PopSpace(nRecv);

		DispatchPackage();
	}
}
#endif

void XSocketStream::DispatchPackage()
{
	while (!m_bUserClosed)
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

		m_DataCallback(pbyData + uHeaderLen, (size_t)uPackageSize);

		m_RecvBuffer.PopData(uHeaderLen + (size_t)uPackageSize);
	}

	m_RecvBuffer.MoveDataToFront();
}

void XSocketStream::SetError(int nError)
{
	if (!m_bErrored)
	{
		m_bErrored = true;
		m_nError = nError ? nError : GetSocketError();
		get_error_string(m_szError, _countof(m_szError), m_nError);
	}
}

void XSocketStream::SetError(const char szUserError[])
{
	if (!m_bErrored)
	{
		safe_cpy(m_szError, szUserError);
		m_bErrored = true;
	}
}
