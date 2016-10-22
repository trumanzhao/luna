#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#include <string.h>
#include "tools.h"
#include "socket_helper.h"

#if defined(__linux) || defined(__APPLE__)
void set_none_block(socket_t fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}
#endif

#ifdef _MSC_VER
void set_none_block(socket_t fd)
{
    u_long  opt = 1;
    ioctlsocket(fd, FIONBIO, &opt);
}
#endif

bool make_ip_addr(sockaddr_storage& addr, const char ip[], int port)
{
	memset(&addr, 0, sizeof(addr));

	if (strchr(ip, ':') || ip[0] == '\0')
	{
		sockaddr_in6* ipv6 = (sockaddr_in6*)&addr;
		ipv6->sin6_family = AF_INET6;
		ipv6->sin6_port = htons(port);
		ipv6->sin6_addr = in6addr_any;
		return ip[0] == '\0' || inet_pton(AF_INET6, ip, &ipv6->sin6_addr) == 1;
	}

	sockaddr_in* ipv4 = (sockaddr_in*)&addr;
	ipv4->sin_family = AF_INET;
	ipv4->sin_port = htons(port);
	return inet_pton(AF_INET, ip, &ipv4->sin_addr) == 1;
}

bool get_ip_string(char ip[], size_t len, const sockaddr_storage& addr)
{
	socklen_t socklen = sizeof(addr);
	if (addr.ss_family == AF_INET)
	{
		sockaddr_in* ipv4 = (sockaddr_in*)&addr;
		return inet_ntop(ipv4->sin_family, &ipv4->sin_addr, ip, len) != nullptr;
	}
	else if (addr.ss_family == AF_INET6)
	{
		sockaddr_in6* ipv6 = (sockaddr_in6*)&addr;
		return inet_ntop(ipv6->sin6_family, &ipv6->sin6_addr, ip, len) != nullptr;
	}
	ip[0] = '\0';
	return false;
}

// http://beej.us/guide/bgnet/output/html/multipage/getaddrinfoman.html
// http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
socket_t ConnectSocket(const char szIP[], int nPort)
{
	socket_t nResult = INVALID_SOCKET;
	int nRetCode = false;
	socket_t nSocket = INVALID_SOCKET;
	//hostent* pHost = NULL;
	sockaddr_storage addr;

	// 名字解析,弄成另外一个单独的接口?
	// getaddrinfo
	//pHost = gethostbyname(szIP);
	//FAILED_JUMP(pHost);

	nRetCode = make_ip_addr(addr, szIP, nPort);
	FAILED_JUMP(nRetCode);

	nSocket = socket(addr.ss_family, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(nSocket != INVALID_SOCKET);

	set_none_block(nSocket);

	nRetCode = connect(nSocket, (sockaddr*)&addr, sizeof(addr));
	if (nRetCode == SOCKET_ERROR)
	{
		nRetCode = get_socket_error();

#ifdef _MSC_VER
		FAILED_JUMP(nRetCode == WSAEWOULDBLOCK);
#endif

#if defined(__linux) || defined(__APPLE__)
		FAILED_JUMP(nRetCode == EINPROGRESS);
#endif
	}

	nResult = nSocket;
Exit0:
	if (nResult == INVALID_SOCKET)
	{
		if (nSocket != INVALID_SOCKET)
		{
			close_socket_handle(nSocket);
			nSocket = INVALID_SOCKET;
		}
	}
	return nResult;
}

bool check_can_write(socket_t fd, int timeout)
{
	timeval tv = { timeout / 1000, 1000 * (timeout % 1000) };
	fd_set wset;

	FD_ZERO(&wset);
	FD_SET(fd, &wset);

	return select((int)fd + 1, nullptr, &wset, nullptr, timeout >= 0 ? &tv : nullptr) == 1;
}
