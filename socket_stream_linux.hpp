
XSocketStream::XSocketStream(XSocketManager* pMgr)
{
	m_pMgr = pMgr;
	m_szError[0] = 0;
	m_Event = [this](bool bRead, bool bWrite)
	{ 
		if (bRead) Recv();
		if (bWrite) SendCacheData();
	};	
}

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
	size_t uHeaderLen = EncodeU64(byHeader, sizeof(byHeader), uDataLen);

	StreamSend(byHeader, uHeaderLen);
	StreamSend(pvData, uDataLen);
}

void XSocketStream::Close()
{
	if (m_nSocket != INVALID_SOCKET)
	{
		CloseSocketHandle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
	m_bUserClosed = true;
}

void XSocketStream::StreamSend(const void* pvData, size_t uDataLen)
{
	if (m_bErrored)
		return;

	if (!m_bWriteAble)
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
			if (nErr == EINTR)
				continue;

			if (nErr == EAGAIN)
			{
				m_bWriteAble = false;

				if (!m_SendBuffer.PushData(pbyData, uDataLen))
				{
					SetError("Send cache is full !");
					return;
				}

				if (!m_pMgr->WatchForSend(this, true))
				{
					SetError();
				}
				return;
			}

			SetError(nErr);
			return;
		}

		pbyData += nSend;
		uDataLen -= nSend;
	}
}

void XSocketStream::SendCacheData()
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
			if (!m_pMgr->WatchForSend(this, false))
			{
				SetError();
			}
			break;
		}

		size_t uTryLen = Min(MAX_SIZE_PER_SEND, uDataLen);
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

void XSocketStream::Recv()
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

	m_RecvBuffer.MoveDataToFront();
}

void XSocketStream::SetError(int nError)
{
	if (m_bErrored)
		return;
	m_bErrored = true;
	m_nError = nError ? nError : GetSocketError();

	m_szError[0] = 0;
	const char* pszError = strerror(m_nError);
	if (pszError)
	{
		strncpy(m_szError, pszError, _countof(m_szError));
		m_szError[_countof(m_szError) - 1] = 0;
	}
}

void XSocketStream::SetError(const char szUserError[])
{
	if (m_bErrored)
		return;
	m_bErrored = true;
	strncpy(m_szError, szUserError, _countof(m_szError));
	m_szError[_countof(m_szError) - 1] = 0;
}
