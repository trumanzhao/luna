#pragma once

struct XSocketManager : ISocketManager
{
	XSocketManager(HANDLE hCompletionPort);
	~XSocketManager();

	ISocketListener* Listen(const char szIP[], int nPort) override;
	void ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream* pSocket)>& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize) override;
	void Query(int nTimeout = 16) override;
	const char* GetError(int* pnError) override;
	void Release() override { delete this; }

	void SetError(int nError = 0);
	void SetError(const char szUserError[]);

	void ProcessAsyncConnect();
	ISocketStream* CreateStreamSocket(SOCKET nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP);
	void ProcessSocketEvent(int nTimeout);

	HANDLE m_hCompletionPort = INVALID_HANDLE_VALUE;
	OVERLAPPED_ENTRY m_Events[1024];
	char m_szError[128];
	int m_nLastError = 0;

	std::list<XConnectingStream> m_ConnectingQueue;
	std::list<struct XSocketStream*> m_StreamTable;
	std::list<struct XSocketListener*> m_ListenTable;
};
