/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
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

static char s_zero = 0;
bool wsa_send_empty(socket_t fd, WSAOVERLAPPED& ovl)
{
    DWORD bytes = 0;
    WSABUF ws_buf = { 0, &s_zero };

    memset(&ovl, 0, sizeof(ovl));
    int ret = WSASend(fd, &ws_buf, 1, &bytes, 0, &ovl, nullptr);
    if (ret == 0)
    {
        return true;
    }
    else if (ret == SOCKET_ERROR)
    {
        int err = get_socket_error();
        if (err == WSA_IO_PENDING)
        {
            return true;
        }
    }
    return false;
}

bool wsa_recv_empty(socket_t fd, WSAOVERLAPPED& ovl)
{
    DWORD bytes = 0;
    DWORD flags = 0;
    WSABUF ws_buf = { 0, &s_zero };

    memset(&ovl, 0, sizeof(ovl));
    int ret = WSARecv(fd, &ws_buf, 1, &bytes, &flags, &ovl, nullptr);
    if (ret == 0)
    {
        return true;
    }
    else if (ret == SOCKET_ERROR)
    {
        int err = get_socket_error();
        if (err == WSA_IO_PENDING)
        {
            return true;
        }
    }
    return false;
}
#endif

bool make_ip_addr(sockaddr_storage* addr, size_t* len, const char ip[], int port)
{
    if (strchr(ip, ':'))
    {
        sockaddr_in6* ipv6 = (sockaddr_in6*)addr;
        memset(ipv6, 0, sizeof(*ipv6));
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(port);
        ipv6->sin6_addr = in6addr_any;
        *len = sizeof(*ipv6);
        return ip[0] == '\0' || inet_pton(AF_INET6, ip, &ipv6->sin6_addr) == 1;
    }

    sockaddr_in* ipv4 = (sockaddr_in*)addr;
    memset(ipv4, 0, sizeof(*ipv4));
    ipv4->sin_family = AF_INET;
    ipv4->sin_port = htons(port);
    ipv4->sin_addr.s_addr = INADDR_ANY;
    *len = sizeof(*ipv4);
    return ip[0] == '\0' || inet_pton(AF_INET, ip, &ipv4->sin_addr) == 1;
}

bool get_ip_string(char ip[], size_t ip_size, const void* addr, size_t addr_len)
{
    auto* saddr = (sockaddr*)addr;

    ip[0] = '\0';

    if (addr_len >= sizeof(sockaddr_in) && saddr->sa_family == AF_INET)
    {
        auto* ipv4 = (sockaddr_in*)addr;
        return inet_ntop(ipv4->sin_family, &ipv4->sin_addr, ip, ip_size) != nullptr;
    }
    else if (addr_len >= sizeof(sockaddr_in6) && saddr->sa_family == AF_INET6)
    {
        auto* ipv6 = (sockaddr_in6*)addr;
        return inet_ntop(ipv6->sin6_family, &ipv6->sin6_addr, ip, ip_size) != nullptr;
    }
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
