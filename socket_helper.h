/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#if defined(__linux) || defined(__APPLE__)
#include <errno.h>
using socket_t = int;
const socket_t INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
inline int get_socket_error() { return errno; }
inline void close_socket_handle(socket_t fd) { close(fd); }
#endif

#ifdef _MSC_VER
using socket_t = SOCKET;
inline int get_socket_error() { return WSAGetLastError(); }
inline void close_socket_handle(socket_t fd) { closesocket(fd); }
bool wsa_send_empty(socket_t fd, WSAOVERLAPPED& ovl);
bool wsa_recv_empty(socket_t fd, WSAOVERLAPPED& ovl);
#endif

bool make_ip_addr(sockaddr_storage* addr, size_t* len, const char ip[], int port);
// ip字符串建议大小: char ip[INET6_ADDRSTRLEN];
bool get_ip_string(char ip[], size_t ip_size, const void* addr, size_t addr_len);

// timeout: 单位ms,传入-1表示阻塞到永远
bool check_can_write(socket_t fd, int timeout);

void set_none_block(socket_t nSocket);


#define MAX_HEADER_LEN	16

// 每次调用send时,最多发送的数据量,如果太大,可能会导致EAGAIN错误
#define MAX_SIZE_PER_SEND	((size_t)1024 * 4)

