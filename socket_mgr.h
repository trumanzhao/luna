#pragma once

#include <list>
#include <vector>
#include <string>

#include "socket_helper.h"
#include "socket_io.h"

struct XSocketManager : ISocketManager
{
	XSocketManager();
	~XSocketManager();

	bool Setup(int max_connection);

	ISocketListener* Listen(const char szIP[], int nPort) override;
	void ConnectAsync(const char szIP[], int nPort, const connecting_callback_t& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize) override;
	void Wait(int nTimeout = 16) override;
	void Release() override { delete this; }

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

	char m_szError[64];
	int m_max_connection = 0;

	struct XAsyncConnecting
	{
		connecting_callback_t callback;
		int nTimeout;
		size_t uRecvBufferSize;
		size_t uSendBufferSize;
		int64_t dwBeginTime;
		socket_t nSocket;
		std::string strRemoteIP;
		int nPort;
	};
	std::list<XAsyncConnecting> m_ConnectingQueue;
	std::list<struct XSocketStream*> m_StreamTable;
	std::list<struct XSocketListener*> m_ListenTable;
};
