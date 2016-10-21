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
#include <string.h>
#include "tools.h"
#include "socket_mgr.h"
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
		close_socket_handle(m_nSocket);
		m_nSocket = INVALID_SOCKET;
	}
}
// todo: ipv6 support
// http://www.ibm.com/support/knowledgecenter/ssw_i5_54/rzab6/xacceptboth.htm
void XSocketListener::TryAccept()
{
	sockaddr_in6 clientaddr;
	char ip[INET6_ADDRSTRLEN];

	while (!m_bUserClosed)
	{
		socket_t nSocket = INVALID_SOCKET;
		socklen_t nAddrLen = sizeof(clientaddr);

		memset(&clientaddr, 0, sizeof(clientaddr));

		nSocket = accept(m_nSocket, (sockaddr*)&clientaddr, &nAddrLen);
		if (nSocket == INVALID_SOCKET)
			break;

		set_none_block(nSocket);

		inet_ntop(AF_INET6, &clientaddr.sin6_addr, ip, sizeof(ip));

		ISocketStream* pStream = m_pSocketMgr->CreateStreamSocket(nSocket, m_uStreamRecvBufferSize, m_uStreamSendBufferSize, ip);
		if (pStream == nullptr)
		{
			close_socket_handle(nSocket);
			continue;
		}

		m_StreamCallback(pStream);
	}
}
