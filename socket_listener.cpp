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
	m_socket = nSocket;
}

XSocketListener::~XSocketListener()
{
	if (m_socket != INVALID_SOCKET)
	{
		close_socket_handle(m_socket);
		m_socket = INVALID_SOCKET;
	}
}
// todo: ipv6 support
// http://www.ibm.com/support/knowledgecenter/ssw_i5_54/rzab6/xacceptboth.htm
void XSocketListener::TryAccept()
{
	while (!m_user_closed)
	{
		sockaddr_storage addr;
		socket_t fd = INVALID_SOCKET;
		socklen_t addr_len = sizeof(addr);
		char ip[INET6_ADDRSTRLEN];

		memset(&addr, 0, sizeof(addr));
		fd = accept(m_socket, (sockaddr*)&addr, &addr_len);
		if (fd == INVALID_SOCKET)
			break;

		set_none_block(fd);
		get_ip_string(ip, sizeof(ip), addr);

		ISocketStream* pStream = m_pSocketMgr->CreateStreamSocket(fd, m_recv_buffer_size, m_send_buffer_size, ip);
		if (pStream == nullptr)
		{
			close_socket_handle(fd);
			continue;
		}

		m_StreamCallback(pStream);
	}
}
