#pragma once

struct XSocketManager;

struct XSocketStream : ISocketStream
{
	XSocketStream(XSocketManager* pMgr);
	~XSocketStream();

	void Activate();
	void Send(const void* pvData, size_t uDataLen) override;
	void SetDataCallback(const std::function<void(BYTE* pbyData, size_t uDataLen)>& callback) override { m_DataCallback = callback; }
	void SetErrorCallback(const std::function<void()>& callback) override { m_ErrorCallback = callback; }
	const char* GetRemoteAddress() override { return m_strRemoteIP.c_str(); }
	const char* GetError(int* pnError = nullptr) override { if (pnError) *pnError = m_nError; return m_szError; };
	void Close() override;

	void StreamSend(const void* pvData, size_t uDataLen);
	void WatchForSend();
	void SendCacheData(int64_t nLen);

	void Recv(int64_t nLen);
	void DispatchPackage();

	void SetError(int nError = 0);
	void SetError(const char szUserError[]);

	std::string m_strRemoteIP;
	XSocketManager* m_pMgr = nullptr;
	SOCKET m_nSocket = INVALID_SOCKET;
	bool m_bErrored = false;
	int  m_nError = 0;
	char m_szError[120];
	bool m_bErrorNotified = false;
	bool m_bUserClosed = false;
	bool m_bWriteAble = true;
	XSocketBuffer m_RecvBuffer;
	XSocketBuffer m_SendBuffer;

	std::function<void(int64_t)> m_RecvEvent;
	std::function<void(int64_t)> m_SendEvent;

	std::function<void(BYTE* pbyData, size_t uDataLen)> m_DataCallback;
	std::function<void()> m_ErrorCallback;
};
