
XSocketStream::XSocketStream(XSocketManager* pMgr)
{
	m_pMgr = pMgr;
	m_szError[0] = 0;
	m_RecvEvent = [this](int64_t nLen){ Recv(nLen);};	
	// m_SendEvent要在需要的时候才赋值
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
				WatchForSend();
				return;
			}
			SetError(nErr);
			return;
		}

		pbyData += nSend;
		uDataLen -= nSend;
	}
}

void XSocketStream::WatchForSend()
{
	if (m_SendEvent)
	{
		m_pMgr->EnableSendWatch(m_nSocket, &m_SendEvent, true);
		return;
	}

	// 第一次watch...
	m_SendEvent = [this](int64_t nLen){ SendCacheData(nLen); };
	m_pMgr->WatchForSend(m_nSocket, &m_SendEvent);
}

void XSocketStream::SendCacheData(int64_t nLen)
{
	BYTE* pbyData = nullptr;
	size_t uDataLen = 0;

	if (m_bErrored || m_bUserClosed)
		return;

	if (nLen <= 0)
	{
		SetError("Send 0, connection lost !");
		return;
	}

	pbyData = m_SendBuffer.GetData(&uDataLen);
	if (uDataLen < nLen)
	{
		m_bWriteAble = true;
		m_pMgr->EnableSendWatch(m_nSocket, &m_SendEvent, false);		
	}
	else
	{
		uDataLen = (size_t)nLen;		
	}

	m_SendBuffer.PopData(uDataLen);

	while (uDataLen > 0)
	{
		size_t uTryLen = Min(MAX_SIZE_PER_SEND, uDataLen);
		int nSend = send(m_nSocket, pbyData, uTryLen, 0);
		if (nSend == -1)
		{
			int nErr = GetSocketError();
			if (nErr == EINTR)
				continue;

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

	m_SendBuffer.MoveDataToFront();
}

void XSocketStream::Recv(int64_t nLen)
{
	if (m_bErrored || m_bUserClosed)
		return;

	if (nLen <= 0)
	{
		SetError("Recv 0, connection lost !");
		return;
	}	
	
	while (nLen > 0 && !m_bUserClosed)
	{
		size_t uSpaceSize = 0;
		BYTE* pbySpace = m_RecvBuffer.GetSpace(&uSpaceSize);

		if (uSpaceSize == 0)
		{
			SetError("Recv package too large !");
			return;
		}

		int nRecv = recv(m_nSocket, pbySpace, Min(uSpaceSize, (size_t)nLen), 0);
		if (nRecv == -1)
		{
			int nErr = GetSocketError();
			if (nErr == EINTR)
				continue;

			SetError(nErr);
			return;
		}

		if (nRecv == 0)
		{
			SetError("Recv 0, connection lost !");
			return;
		}

		m_RecvBuffer.PopSpace(nRecv);
		nLen -= nRecv;

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
