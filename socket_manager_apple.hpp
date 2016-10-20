XSocketManager::XSocketManager()
{
	m_szError[0] = 0;
}

XSocketManager::~XSocketManager()
{
	Clear();
}

bool XSocketManager::Setup()
{
    bool			bResult		= false;
    sig_t			pHandler	= NULL;

    pHandler = signal(SIGPIPE, SIG_IGN);
    FAILED_JUMP(pHandler != SIG_ERR);

	m_nKQ = kqueue();
	FAILED_JUMP(m_nKQ != -1);

    bResult = true;
Exit0:
    return bResult;
}

void XSocketManager::Clear()
{
	SAFE_CLOSE_FD(m_nKQ);

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
}

ISocketListener* XSocketManager::Listen(const char szIP[], int nPort)
{
	ISocketListener*    pResult		= nullptr;
	int                 nRetCode	= false;
	int                 nOne		= 1;
	unsigned long       ulAddress	= INADDR_ANY;
	SOCKET              nSocket		= INVALID_SOCKET;
	XSocketListener*	pSocket		= nullptr;
	sockaddr_in         localAddr;
	struct kevent		event;

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

	EV_SET(&event, nSocket, EVFILT_READ, EV_ADD, 0, 0, &pSocket->m_AcceptEvent);
	kevent(m_nKQ, &event, 1, nullptr, 0, nullptr);

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

		if (pStream->m_bUserClosed)
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
	const char* pszError = strerror(m_nLastError);
	if (pszError)
	{
		strncpy(m_szError, pszError, _countof(m_szError));
		m_szError[_countof(m_szError) - 1] = 0;
	}
}

void XSocketManager::SetError(const char szUserError[])
{
	m_nLastError = 0;
	strncpy(m_szError, szUserError, _countof(m_szError));
	m_szError[_countof(m_szError) - 1] = 0;
}

void XSocketManager::ProcessSocketEvent(int nTimeout)
{
    int			nCount		= 0;
	timespec*	pTimeWait	= nullptr;
	timespec	timeWait;

	if (nTimeout >= 0)
	{
		timeWait.tv_sec = nTimeout / 1000;
		timeWait.tv_nsec = (nTimeout % 1000) * 1000000;
		pTimeWait = &timeWait;
	}

	nCount = kevent(m_nKQ, nullptr, 0, m_Events, _countof(m_Events), pTimeWait);

	for (int i = 0; i < nCount; i++)
    {
		struct kevent& ev = m_Events[i];

		assert(ev.filter == EVFILT_READ || ev.filter == EVFILT_WRITE);

		auto& callback = *(std::function<void(size_t)>*)ev.udata;
		callback(ev.data);
    }

}

ISocketStream* XSocketManager::CreateStreamSocket(SOCKET nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP)
{
	struct kevent event;
	XSocketStream* pStream = new XSocketStream(this);

	SetSocketNoneBlock(nSocket);

	EV_SET(&event, nSocket, EVFILT_READ, EV_ADD, 0, 0, &pStream->m_RecvEvent);
	kevent(m_nKQ, &event, 1, nullptr, 0, nullptr);

	pStream->m_strRemoteIP = strRemoteIP;
	pStream->m_nSocket = nSocket;
	pStream->m_RecvBuffer.SetSize(uRecvBufferSize);
	pStream->m_SendBuffer.SetSize(uSendBufferSize);
	m_StreamTable.push_back(pStream);

	return pStream;
}


void XSocketManager::WatchForSend(int nSocket, std::function<void(int64_t)>* pEventHandler)
{
	struct kevent event;
	EV_SET(&event, nSocket, EVFILT_WRITE, EV_ADD, 0, 0, pEventHandler);
	kevent(m_nKQ, &event, 1, nullptr, 0, nullptr);
}

void XSocketManager::EnableSendWatch(int nSocket, std::function<void(int64_t)>* pEventHandler, bool bEnable)
{
	struct kevent event;
	EV_SET(&event, nSocket, EVFILT_WRITE, bEnable ? EV_ENABLE : EV_DISABLE, 0, 0, pEventHandler);
	kevent(m_nKQ, &event, 1, nullptr, 0, nullptr);
}


