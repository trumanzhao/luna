#include "Base.h"
#include "SocketHelper.h"
#include "SocketBuffer.h"
#include "SocketMgr.h"

struct XConnectingStream
{
	std::function<void(ISocketStream* pSocket)> callback;
	int nTimeout;
	size_t uRecvBufferSize;
	size_t uSendBufferSize;
	DWORD dwBeginTime;
	SOCKET nSocket;
	std::string strRemoteIP;
	int nPort;
};

#ifdef _MSC_VER
#include "SocketStreamWindows.h"
#include "SocketListenerWindows.h"
#include "SocketManagerWindows.h"

#include "SocketStreamWindows.hpp"
#include "SocketListenerWindows.hpp"
#include "SocketManagerWindows.hpp"
#endif


#ifdef __linux
#include "SocketStreamLinux.h"
#include "SocketListenerLinux.h"
#include "SocketManagerLinux.h"

#include "SocketStreamLinux.hpp"
#include "SocketListenerLinux.hpp"
#include "SocketManagerLinux.hpp"
#endif

#ifdef __APPLE__
#include "SocketStreamApple.h"
#include "SocketListenerApple.h"
#include "SocketManagerApple.h"

#include "SocketStreamApple.hpp"
#include "SocketListenerApple.hpp"
#include "SocketManagerApple.hpp"
#endif


void XSocketManager::ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream* pSocket)>& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize)
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

void XSocketManager::ProcessAsyncConnect()
{
	DWORD dwTimeNow = get_time_ms();

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

		if (it->nTimeout >= 0 && dwTimeNow - it->dwBeginTime > (DWORD)it->nTimeout)
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
