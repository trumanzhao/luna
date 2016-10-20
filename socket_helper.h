#pragma once

#if defined(__linux) || defined(__APPLE__)
#include <errno.h>
using socket_t = int;
const socket_t INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
inline int GetSocketError() { return errno; }
inline void CloseSocketHandle(socket_t nSocket) { close(nSocket); }
#endif

#ifdef _MSC_VER
using socket_t = SOCKET;
inline int GetSocketError() { return WSAGetLastError(); }
inline void CloseSocketHandle(socket_t nSocket) { closesocket(nSocket); }
#endif

// 返回的Socket未必是已经完成连接的,还需用CheckSocketConnected检查
// 返回的Socket已经设置为异步模式
socket_t ConnectSocket(const char szIP[], int nPort);

// nTimeout: 单位ms,传入-1表示阻塞到永远
bool CheckSocketWriteable(int* pnError, socket_t nSocket, int nTimeout);

void SetSocketNoneBlock(socket_t nSocket);


#define MAX_HEADER_LEN	16

// 每次调用send时,最多发送的数据量,如果太大,可能会导致EAGAIN错误
#define MAX_SIZE_PER_SEND	((size_t)1024 * 4)

