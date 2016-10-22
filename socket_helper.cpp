#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#if defined(__linux) || defined(__APPLE__)
#include <arpa/inet.h>
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

bool make_ip_addr(sockaddr_storage* addr, size_t* len, const char ip[], int port)
{
	if (strchr(ip, ':') || ip[0] == '\0')
	{
		sockaddr_in6* ipv6 = (sockaddr_in6*)addr;
		ipv6->sin6_family = AF_INET6;
		ipv6->sin6_port = htons(port);
		ipv6->sin6_addr = in6addr_any;
		*len = sizeof(*ipv6);
		return ip[0] == '\0' || inet_pton(AF_INET6, ip, &ipv6->sin6_addr) == 1;
	}

	sockaddr_in* ipv4 = (sockaddr_in*)addr;
	ipv4->sin_family = AF_INET;
	ipv4->sin_port = htons(port);
	*len = sizeof(*ipv4);
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

bool check_can_write(socket_t fd, int timeout)
{
	timeval tv = { timeout / 1000, 1000 * (timeout % 1000) };
	fd_set wset;

	FD_ZERO(&wset);
	FD_SET(fd, &wset);

	return select((int)fd + 1, nullptr, &wset, nullptr, timeout >= 0 ? &tv : nullptr) == 1;
}
