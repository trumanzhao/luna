#pragma once

#include <list>
#include <vector>
#include <string>

#include "socket_helper.h"
#include "socket_io.h"

struct XConnectingStream
{
	std::function<void(ISocketStream*)> callback;
	int nTimeout;
	size_t uRecvBufferSize;
	size_t uSendBufferSize;
	int64_t dwBeginTime;
	socket_t nSocket;
	std::string strRemoteIP;
	int nPort;
};

struct XSocketManager : ISocketManager
{
	XSocketManager();
	~XSocketManager();

	bool Setup(int max_connection);

	ISocketListener* Listen(const char szIP[], int nPort) override;
	void ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream*)>& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize) override;
	void Query(int nTimeout = 16) override;
	const char* GetError(int* pnError) override;
	void Release() override { delete this; }

	void SetError(int nError = 0);
	void SetError(const char szUserError[]);

	void ProcessAsyncConnect();
	ISocketStream* CreateStreamSocket(socket_t nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP);
	void ProcessSocketEvent(int nTimeout);

#ifdef _MSC_VER
	HANDLE m_hCompletionPort = INVALID_HANDLE_VALUE;
	std::vector<OVERLAPPED_ENTRY> m_Events;
#endif

#ifdef __linux
	int m_nEpoll = -1;
	std::vector<epoll_event> m_Events;
#endif

#ifdef __APPLE__
	int m_nKQ = -1;
	std::vector<struct kevent> m_Events;
#endif

	char m_szError[128];
	int m_nError = 0;
	int m_max_connection = 0;

	std::list<XConnectingStream> m_ConnectingQueue;
	std::list<struct XSocketStream*> m_StreamTable;
	std::list<struct XSocketListener*> m_ListenTable;
};
