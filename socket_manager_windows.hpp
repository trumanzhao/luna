XSocketManager::XSocketManager(HANDLE hCompletionPort)
{
	WORD    wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;

	WSAStartup(wVersion, &wsaData);

	m_hCompletionPort = hCompletionPort;
	m_szError[0] = 0;
}

XSocketManager::~XSocketManager()
{
	CloseHandle(m_hCompletionPort);

	for (auto pStreamSocket : m_StreamTable)
	{
		delete pStreamSocket;
	}
	m_StreamTable.clear();

	for (auto pListenSocket : m_ListenTable)
	{
		delete pListenSocket;
	}
	m_ListenTable.clear();

	for (auto it : m_ConnectingQueue)
	{
		CloseSocketHandle(it.nSocket);
	}
	m_ConnectingQueue.clear();

	WSACleanup();
}

ISocketListener* XSocketManager::Listen(const char szIP[], int nPort)
{
	ISocketListener*	pResult		= nullptr;
	int                 nRetCode	= false;
	int                 nOne		= 1;
	unsigned long       ulAddress	= INADDR_ANY;
	SOCKET              nSocket		= INVALID_SOCKET;
	XSocketListener*	pSocket		= nullptr;
	sockaddr_in         localAddr;

	nSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(nSocket != INVALID_SOCKET);

	SetSocketNoneBlock(nSocket);

	if (szIP[0] != '\0')
	{
		ulAddress = inet_addr(szIP);
		if (ulAddress == INADDR_NONE)
			ulAddress = INADDR_ANY;
	}

	nRetCode = setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOne, sizeof(nOne));
	FAILED_JUMP(nRetCode != SOCKET_ERROR);

	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = ulAddress;
	localAddr.sin_port = htons(nPort);

	nRetCode = bind(nSocket, (sockaddr*)&localAddr, sizeof(localAddr));
	FAILED_JUMP(nRetCode != SOCKET_ERROR);

	nRetCode = listen(nSocket, 8);
	FAILED_JUMP(nRetCode != SOCKET_ERROR);

	pSocket = new XSocketListener(this, nSocket);
	m_ListenTable.push_back(pSocket);

	pResult = pSocket;
Exit0:
	if (pResult == nullptr)
	{
		SetError();

		if (nSocket != INVALID_SOCKET)
		{
			CloseSocketHandle(nSocket);
			nSocket = INVALID_SOCKET;
		}
		SAFE_DELETE(pSocket);
	}
	return pResult;
}

void XSocketManager::Query(int nTimeout)
{
	ProcessSocketEvent(nTimeout);

	ProcessAsyncConnect();

	auto stream_it = m_StreamTable.begin();
	while (stream_it != m_StreamTable.end())
	{
		XSocketStream* pStream = *stream_it;

		pStream->Activate();

		if (pStream->m_bUserClosed && pStream->m_bRecvComplete && pStream->m_bSendComplete)
		{
			delete pStream;
			stream_it = m_StreamTable.erase(stream_it);
			continue;
		}
		++stream_it;
	}

	auto listen_it = m_ListenTable.begin();
	while (listen_it != m_ListenTable.end())
	{
		XSocketListener* pListenSocket = *listen_it;
		pListenSocket->TryAccept();
		if (pListenSocket->m_bUserClosed)
		{
			delete pListenSocket;
			listen_it = m_ListenTable.erase(listen_it);
			continue;
		}
		++listen_it;
	}
}


const char* XSocketManager::GetError(int* pnError)
{
	if (pnError)
	{
		*pnError = m_nLastError;
	}
	return m_szError;
}

void XSocketManager::SetError(int nError)
{
	m_nLastError = nError ? nError : GetSocketError();
	m_szError[0] = 0;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, m_nLastError, 0, m_szError, sizeof(m_szError), nullptr);
}

void XSocketManager::SetError(const char szUserError[])
{
	m_nLastError = 0;
	strncpy(m_szError, szUserError, _countof(m_szError));
	m_szError[_countof(m_szError) - 1] = 0;
}

ISocketStream* XSocketManager::CreateStreamSocket(SOCKET nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP)
{
	XSocketStream* pStream = new XSocketStream();	
	HANDLE hHandle = CreateIoCompletionPort((HANDLE)nSocket, m_hCompletionPort, (ULONG_PTR)pStream, 0);
	if (hHandle != m_hCompletionPort)
	{
		SetError();
		delete pStream;
		return nullptr;
	}

	pStream->m_strRemoteIP = strRemoteIP;
	pStream->m_nSocket = nSocket;
	pStream->m_RecvBuffer.SetSize(uRecvBufferSize);
	pStream->m_SendBuffer.SetSize(uSendBufferSize);
	m_StreamTable.push_back(pStream);

	pStream->AsyncRecv();

	return pStream;
}

void XSocketManager::ProcessSocketEvent(int nTimeout)
{
	int nRetCode;
	ULONG uEventCount;

	nRetCode = GetQueuedCompletionStatusEx(m_hCompletionPort, m_Events, _countof(m_Events), &uEventCount, (DWORD)nTimeout, false);
	if (!nRetCode)
		return;

	for (ULONG i = 0; i < uEventCount; i++)
	{
		OVERLAPPED_ENTRY& oe = m_Events[i];
		XSocketStream* pStream = (XSocketStream*)oe.lpCompletionKey;
		pStream->OnComplete(oe.lpOverlapped, oe.dwNumberOfBytesTransferred);
	}
}

ISocketManager* CreateSocketManager()
{
	HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hCompletionPort == INVALID_HANDLE_VALUE)
		return nullptr;

	return new XSocketManager(hCompletionPort);
}
