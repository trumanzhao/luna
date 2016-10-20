
XSocketListener::XSocketListener(XSocketManager* pMgr, SOCKET nSocket)
{
	m_pSocketMgr = pMgr;
	m_nSocket = nSocket;
}

XSocketListener::~XSocketListener()
{
	if (m_nSocket != INVALID_SOCKET)
	{
		CloseSocketHandle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
}

void XSocketListener::TryAccept()
{
	sockaddr_in remoteAddr;

	while (!m_bUserClosed)
	{
		SOCKET         nSocket = INVALID_SOCKET;
		int            nAddrLen = sizeof(sockaddr_in);

		memset(&remoteAddr, 0, sizeof(sockaddr_in));

		nSocket = accept(m_nSocket, (sockaddr*)&remoteAddr, &nAddrLen);
		if (nSocket == INVALID_SOCKET)
			break;

		SetSocketNoneBlock(nSocket);

		ISocketStream* pStream = m_pSocketMgr->CreateStreamSocket(nSocket, m_uStreamRecvBufferSize, m_uStreamSendBufferSize, inet_ntoa(remoteAddr.sin_addr));
		if (pStream == nullptr)
		{
			closesocket(nSocket);
			continue;
		}

		m_StreamCallback(pStream);
	}
}
