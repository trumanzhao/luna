XSocketStream::~XSocketStream()
{
	CloseSocketHandle(m_nSocket);
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
	size_t uHeaderLen = EncodeU64(byHeader, sizeof(byHeader), uDataLen);

	StreamSend(byHeader, uHeaderLen);
	StreamSend(pvData, uDataLen);
}

void XSocketStream::Close()
{
	m_bUserClosed = true;
}

void XSocketStream::StreamSend(const void* pvData, size_t uDataLen)
{
	if (m_bErrored)
		return;

	if (!m_bSendComplete)
	{
		if (!m_SendBuffer.PushData(pvData, uDataLen))
		{
			SetError("Send cache is full !");
		}
		return;
	}

	const char*	pbyData = (char*)pvData;
	while (uDataLen > 0)
	{
		size_t uTryLen = Min(MAX_SIZE_PER_SEND, uDataLen);
		int nSend = send(m_nSocket, pbyData, uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = GetSocketError();
			if (nErr == WSAEWOULDBLOCK)
			{
				if (!m_SendBuffer.PushData(pbyData, uDataLen))
				{
					SetError("Send cache is full !");
					return;
				}
				AsyncSend();
				return;
			}
			SetError(nErr);
			return;
		}

		if (nSend == 0)
		{
			SetError("Send 0, connection lost !");
			return;
		}

		pbyData += nSend;
		uDataLen -= nSend;
	}
}

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
		SetError("Recv package too large !");
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
		SetError("Recv 0, connection lost !");
		return;
	}

	m_RecvBuffer.PopSpace(uLen);

	DispatchPackage();

	m_RecvBuffer.MoveDataToFront();

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
		SetError("Send 0, connection lost !");
		return;
	}

	m_SendBuffer.PopData(uLen);
	m_SendBuffer.MoveDataToFront();

	AsyncSend();
}

void XSocketStream::DispatchPackage()
{
	while (!m_bUserClosed)
	{
		BYTE* pbyData = nullptr;
		size_t uDataLen = 0;

		pbyData = m_RecvBuffer.GetData(&uDataLen);

		uint64_t uPackageSize = 0;
		size_t uHeaderLen = DecodeU64(&uPackageSize, pbyData, uDataLen);
		if (uHeaderLen == 0)
			break;

		// 数据包还没有收完整
		if (uDataLen < uHeaderLen + uPackageSize)
			break;

		m_DataCallback(pbyData + uHeaderLen, (size_t)uPackageSize);

		m_RecvBuffer.PopData(uHeaderLen + (size_t)uPackageSize);
	}
}

void XSocketStream::SetError(int nError)
{
	if (m_bErrored)
		return;
	m_bErrored = true;
	m_nError = nError ? nError : GetSocketError();
	m_szError[0] = 0;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, m_nError, 0, m_szError, sizeof(m_szError), nullptr);
}

void XSocketStream::SetError(const char szUserError[])
{
	if (m_bErrored)
		return;
	m_bErrored = true;
	strncpy(m_szError, szUserError, _countof(m_szError));
	m_szError[_countof(m_szError) - 1] = 0;
}
