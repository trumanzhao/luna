#pragma once

#include <functional>

struct ISocketStream 
{
	virtual void Send(const void* pvData, size_t uDataLen) = 0;
	virtual void SetDataCallback(const std::function<void(BYTE* pbyData, size_t uDataLen)>& callback) = 0;
	virtual void SetErrorCallback(const std::function<void()>& callback) = 0;
	virtual const char* GetRemoteAddress() = 0;
	virtual const char* GetError(int* pnError = nullptr) = 0;
	virtual void Close() = 0;
};

struct ISocketListener
{
	virtual void SetStreamCallback(const std::function<void(ISocketStream* pSocket)>& callback) = 0;
	// 设计不希望StreamSocket在运行中动态修改缓冲区大小,所以只能在这里设置
	virtual void SetStreamBufferSize(size_t uRecvBufferSize, size_t uSendBufferSize) = 0;
	virtual void Close() = 0;
};

struct ISocketManager
{
	virtual ISocketListener* Listen(const char szIP[], int nPort) = 0;
	// nTimeout: 单位ms,-1表示无限
	// uSendBufferSize,指网络库内部使用的发送缓冲区大小
	// uRecvBufferSize,指网络库内部使用的缓冲区大小,至少要能容纳一个最大网络包
	// 无论成败,回调一定会发生,如果连接失败,可以通过GetError获取错误信息
	virtual void ConnectAsync(const char szIP[], int nPort, const std::function<void(ISocketStream* pSocket)>& callback, int nTimeout = 2000, size_t uRecvBufferSize = 4096, size_t uSendBufferSize = 4096) = 0;

	// nTimeout: 单位ms,-1表示无限
	virtual void Query(int nTimeout = 50) = 0;

	// 不能来判断"是否出错",只有出错后才有意义
	virtual const char* GetError(int* pnError) = 0;

	virtual void Release() = 0;
};

ISocketManager* CreateSocketManager();
