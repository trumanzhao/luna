#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#ifdef __linux
#include <sys/epoll.h>
#endif
#if defined(__linux) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <string.h>
#include "tools.h"
#include "socket_mgr_impl.h"
#include "socket_listener.h"

XSocketListener::XSocketListener(XSocketManager* pMgr, socket_t nSocket)
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
		socket_t nSocket = INVALID_SOCKET;
		socklen_t nAddrLen = sizeof(sockaddr_in);

		memset(&remoteAddr, 0, sizeof(sockaddr_in));

		nSocket = accept(m_nSocket, (sockaddr*)&remoteAddr, &nAddrLen);
		if (nSocket == INVALID_SOCKET)
			break;

		SetSocketNoneBlock(nSocket);

		ISocketStream* pStream = m_pSocketMgr->CreateStreamSocket(nSocket, m_uStreamRecvBufferSize, m_uStreamSendBufferSize, inet_ntoa(remoteAddr.sin_addr));
		if (pStream == nullptr)
		{
			CloseSocketHandle(nSocket);
			continue;
		}

		m_StreamCallback(pStream);
	}
}
