
XSocketListener::XSocketListener(XSocketManager* pMgr, SOCKET nSocket)
{
	m_pSocketMgr = pMgr;
	m_nSocket = nSocket;

	m_Event = [this](bool bRead, bool bWrite)
	{ 
		assert(bRead);
		assert(!bWrite);
		Accept(); 
	};
}

XSocketListener::~XSocketListener()
{
	if (m_nSocket != INVALID_SOCKET)
	{
		CloseSocketHandle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
}

void XSocketListener::Accept()
{
	sockaddr_in remoteAddr;

	while (!m_bUserClosed)
	{
		SOCKET         nSocket = INVALID_SOCKET;
		socklen_t      nAddrLen = sizeof(sockaddr_in);

		memset(&remoteAddr, 0, sizeof(sockaddr_in));

		nSocket = accept(m_nSocket, (sockaddr*)&remoteAddr, &nAddrLen);
		if (nSocket == INVALID_SOCKET)
			break;

		ISocketStream* pStream = m_pSocketMgr->CreateStreamSocket(nSocket, m_uStreamRecvBufferSize, m_uStreamSendBufferSize, inet_ntoa(remoteAddr.sin_addr));
		if (pStream == nullptr)
		{
			CloseSocketHandle(nSocket);
			continue;
		}

		m_StreamCallback(pStream);
	}
}

ISocketManager* CreateSocketManager()
{
	XSocketManager* pMgr = new XSocketManager();
	if (pMgr->Setup())
	{
		return pMgr;
	}
	delete pMgr;
	return nullptr;
}
