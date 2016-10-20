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
#include "tools.h"
#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr_impl.h"
#include "socket_listener.h"
#include "socket_stream.h"

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

XSocketManager::XSocketManager()
{
#ifdef _MSC_VER
	WORD    wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	WSAStartup(wVersion, &wsaData);
#endif

	m_szError[0] = 0;
}

XSocketManager::~XSocketManager()
{
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

#ifdef _MSC_VER
	if (m_hCompletionPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCompletionPort);
		m_hCompletionPort = INVALID_HANDLE_VALUE;
	}
	WSACleanup();
#endif

#ifdef __linux
	if (m_nEpoll != -1)
	{
		close(m_nEpoll);
		m_nEpoll = -1;
	}
#endif
}

bool XSocketManager::Setup(int max_connection)
{
#ifdef _MSC_VER
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_hCompletionPort == INVALID_HANDLE_VALUE)
		return false;
#endif

#ifdef __linux
	m_nEpoll = epoll_create(max_connection);
	if (m_nEpoll == -1)
		return false;
#endif

	m_max_connection = max_connection;
	m_Events.resize(max_connection);
	return true;
}

ISocketListener* XSocketManager::Listen(const char szIP[], int nPort)
{
	ISocketListener*	pResult = nullptr;
	int                 nRetCode = false;
	int                 nOne = 1;
	socket_t            nSocket = INVALID_SOCKET;
	XSocketListener*	pSocket = nullptr;
	sockaddr_in         localAddr;

	nSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(nSocket != INVALID_SOCKET);

	SetSocketNoneBlock(nSocket);

	nRetCode = setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOne, sizeof(nOne));
	FAILED_JUMP(nRetCode != SOCKET_ERROR);

	memset(&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(nPort);
	if (szIP[0] != '\0')
	{
		nRetCode = inet_pton(AF_INET, szIP, &localAddr.sin_addr);
		FAILED_JUMP(nRetCode == 1);
	}

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

void XSocketManager::ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream*)>& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize)
{
	XConnectingStream cs;

	cs.callback = callback;
	cs.nSocket = INVALID_SOCKET;
	cs.dwBeginTime = get_time_ms();
	cs.nTimeout = nTimeout;
	cs.uRecvBufferSize = uRecvBufferSize;
	cs.uSendBufferSize = uSendBufferSize;
	cs.strRemoteIP = szIP;
	cs.nPort = nPort;

	m_ConnectingQueue.push_back(cs);
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

		if (pStream->m_bUserClosed 
#ifdef _MSC_VER
		&& pStream->m_bRecvComplete && pStream->m_bSendComplete
#endif
        )
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

void XSocketManager::ProcessAsyncConnect()
{
	int64_t dwTimeNow = get_time_ms();

	auto it = m_ConnectingQueue.begin();
	while (it != m_ConnectingQueue.end())
	{
		if (it->nSocket == INVALID_SOCKET)
		{
			it->nSocket = ConnectSocket(it->strRemoteIP.c_str(), it->nPort);
			if (it->nSocket == INVALID_SOCKET)
			{
				SetError();
				it->callback(nullptr);
				it = m_ConnectingQueue.erase(it);
				continue;
			}
		}

		int nError = 0;
		if (CheckSocketWriteable(&nError, it->nSocket, 0))
		{
			ISocketStream* pSocket = nullptr;

			if (nError == 0)
			{
				pSocket = CreateStreamSocket(it->nSocket, it->uRecvBufferSize, it->uSendBufferSize, it->strRemoteIP);
			}
			else
			{
				SetError(nError);
			}

			if (pSocket == nullptr)
			{
				CloseSocketHandle(it->nSocket);
			}

			it->callback(pSocket);
			it = m_ConnectingQueue.erase(it);
			continue;
		}

		if (it->nTimeout >= 0 && dwTimeNow - it->dwBeginTime > it->nTimeout)
		{
			SetError("Request timeout !");
			it->callback(nullptr);
			CloseSocketHandle(it->nSocket);
			it = m_ConnectingQueue.erase(it);
			continue;
		}

		++it;
	}
}


const char* XSocketManager::GetError(int* pnError)
{
	if (pnError)
	{
		*pnError = m_nError;
	}
	return m_szError;
}

void XSocketManager::SetError(int nError)
{
	m_nError = nError ? nError : GetSocketError();
	m_szError[0] = 0;
#ifdef _MSC_VER
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, m_nError, 0, m_szError, sizeof(m_szError), nullptr);
#endif

#if defined(__linux) || defined(__APPLE__)
        strerror_r(m_nError, m_szError, sizeof(m_szError));
#endif
}

void XSocketManager::SetError(const char szUserError[])
{
	m_nError = 0;
	safe_cpy(m_szError, szUserError);
}

void XSocketManager::ProcessSocketEvent(int nTimeout)
{
#ifdef _MSC_VER
	ULONG uEventCount;
	int nRetCode = GetQueuedCompletionStatusEx(m_hCompletionPort, &m_Events[0], (ULONG)m_Events.size(), &uEventCount, (DWORD)nTimeout, false);
	if (!nRetCode)
		return;

	for (ULONG i = 0; i < uEventCount; i++)
	{
		OVERLAPPED_ENTRY& oe = m_Events[i];
		XSocketStream* pStream = (XSocketStream*)oe.lpCompletionKey;
		pStream->OnComplete(oe.lpOverlapped, oe.dwNumberOfBytesTransferred);
	}
#endif

#ifdef __linux
	int nCount = epoll_wait(m_nEpoll, m_Events, _countof(m_Events), nTimeout);
	for (int i = 0; i < nCount; i++)
	{
		epoll_event& ev = m_Events[i];
		auto& pStream = (ISocketStream*)ev.data.ptr;

		if (ev.events & EPOLLIN)
		{
			pStream->OnRecvAble();
		}

		if (ev.events & EPOLLOUT)
		{
			pStream->OnSendAble();
		}
	}
#endif

#ifdef __APPLE__
	timespec timeWait;
	timeWait.tv_sec = nTimeout / 1000;
	timeWait.tv_nsec = (nTimeout % 1000) * 1000000;
	int nCount = kevent(m_nKQ, nullptr, 0, &m_Events[0], (int)m_Events.size(), nTimeout >= 0 ? &timeWait : nullptr);
	for (int i = 0; i < nCount; i++)
	{
		struct kevent& ev = m_Events[i];
		auto pStream = (XSocketStream*)ev.udata;
		assert(ev.filter == EVFILT_READ || ev.filter == EVFILT_WRITE);
		if (ev.filter == EVFILT_READ)
		{
			pStream->OnRecvAble();
		}
		else if (ev.filter == EVFILT_WRITE)
		{
			pStream->OnSendAble();
		}
	}
#endif
}

ISocketStream* XSocketManager::CreateStreamSocket(socket_t nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP)
{
	XSocketStream* pStream = new XSocketStream();

	SetSocketNoneBlock(nSocket);

#ifdef _MSC_VER
	HANDLE hHandle = CreateIoCompletionPort((HANDLE)nSocket, m_hCompletionPort, (ULONG_PTR)pStream, 0);
	if (hHandle != m_hCompletionPort)
	{
		SetError();
		delete pStream;
		return nullptr;
	}
#endif

#ifdef __linux
	epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.ptr = pStream;

	int nRetCode = epoll_ctl(m_nEpoll, EPOLL_CTL_ADD, nSocket, &ev);
	if (nRetCode == -1)
	{
		SetError();
		delete pStream;
		return nullptr;
	}
#endif

#ifdef __APPLE__
	struct kevent ev[2];
	// EV_CLEAR 可以边沿触发? 注意读写标志不能按位与
	EV_SET(&ev[0], nSocket, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, pStream);
	EV_SET(&ev[1], nSocket, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, pStream);
	int nRetCode = kevent(m_nKQ, ev, _countof(ev), nullptr, 0, nullptr);
	if (nRetCode == -1)
	{
		SetError();
		delete pStream;
		return nullptr;
	}
#endif

	pStream->m_strRemoteIP = strRemoteIP;
	pStream->m_nSocket = nSocket;
	pStream->m_RecvBuffer.SetSize(uRecvBufferSize);
	pStream->m_SendBuffer.SetSize(uSendBufferSize);
	m_StreamTable.push_back(pStream);

#ifdef _MSC_VER
	pStream->AsyncRecv();
#endif

	return pStream;
}

ISocketManager* create_socket_mgr(int max_connection)
{
	XSocketManager* pMgr = new XSocketManager();
	if (!pMgr->Setup(max_connection))
	{
		delete pMgr;
		return nullptr;
	}
	return pMgr;
}
