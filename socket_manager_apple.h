#pragma once

struct XSocketManager : ISocketManager
{
	XSocketManager();
	~XSocketManager();

	bool Setup();
	void Clear();

	ISocketListener* Listen(const char szIP[], int nPort) override;
	void ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream* pSocket)>& callback, int nTimeout, size_t uRecvBufferSize, size_t uSendBufferSize) override;
	void Query(int nTimeout = 16) override;
	const char* GetError(int* pnError) override;
	void Release() override { delete this; }

	void SetError(int nError = 0);
	void SetError(const char szUserError[]);

	void ProcessAsyncConnect();
	void ProcessSocketEvent(int nTimeout);

	ISocketStream* CreateStreamSocket(SOCKET nSocket, size_t uRecvBufferSize, size_t uSendBufferSize, const std::string& strRemoteIP);

	void WatchForSend(int nSocket, std::function<void(int64_t)>* pEventHandler);
	void EnableSendWatch(int nSocket, std::function<void(int64_t)>* pEventHandler, bool bEnable);

	int m_nKQ = -1;
	struct kevent m_Events[1024];
	char m_szError[128];
	int m_nLastError = 0;

	std::list<XConnectingStream> m_ConnectingQueue;
	std::list<struct XSocketStream*> m_StreamTable;
	std::list<struct XSocketListener*> m_ListenTable;
};
