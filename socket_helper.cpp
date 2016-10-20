#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#endif
#include <string.h>
#include "tools.h"
#include "socket_helper.h"

#if defined(__linux) || defined(__APPLE__)
void SetSocketNoneBlock(socket_t nSocket)
{
    int     nOption  = 0;

    nOption = fcntl(nSocket, F_GETFL, 0);
    fcntl(nSocket, F_SETFL, nOption | O_NONBLOCK);
}
#endif

#ifdef _MSC_VER
void SetSocketNoneBlock(socket_t nSocket)
{
    u_long  ulOption = 1;
    ioctlsocket(nSocket, FIONBIO, &ulOption);
}
#endif

socket_t ConnectSocket(const char szIP[], int nPort)
{
	socket_t              nResult = INVALID_SOCKET;
	int                 nRetCode = false;
	socket_t              nSocket = INVALID_SOCKET;
	hostent*            pHost = NULL;
	sockaddr_in         serverAddr;

	pHost = gethostbyname(szIP);
	FAILED_JUMP(pHost);

	nSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	FAILED_JUMP(nSocket != INVALID_SOCKET);

	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = *(unsigned long*)pHost->h_addr_list[0];
	serverAddr.sin_port = htons(nPort);

	SetSocketNoneBlock(nSocket);

	nRetCode = connect(nSocket, (sockaddr*)&serverAddr, sizeof(sockaddr_in));
	if (nRetCode == SOCKET_ERROR)
	{
		nRetCode = GetSocketError();

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
			CloseSocketHandle(nSocket);
			nSocket = INVALID_SOCKET;
		}
	}
	return nResult;
}

// nTimeout: 单位ms,传入-1表示阻塞到永远
bool CheckSocketWriteable(int* pnError, socket_t nSocket, int nTimeout)
{
	bool bResult = false;
	int	nRetCode = 0;
	int nError = 0;
	socklen_t nSockLen = sizeof(int);
	timeval timeoutValue = { nTimeout / 1000, 1000 * (nTimeout % 1000) };
	fd_set writeSet;

	FD_ZERO(&writeSet);
	FD_SET(nSocket, &writeSet);

	nRetCode = select((int)nSocket + 1, NULL, &writeSet, NULL, &timeoutValue);
	FAILED_JUMP(nRetCode == 1);

	bResult = true;

	nRetCode = getsockopt(nSocket, SOL_SOCKET, SO_ERROR, (char*)&nError, &nSockLen);
	FAILED_JUMP(nRetCode == 0);
	*pnError = nError;

Exit0:
	return bResult;
}
