#pragma once

#include "socket_helper.h"
#include "socket_mgr.h"

struct XSocketManager;

struct XSocketListener : ISocketListener
{
	XSocketListener(XSocketManager* pMgr, socket_t nSocket);
	~XSocketListener();

	void SetStreamCallback(const std::function<void(ISocketStream* pSocket)>& callback) override { m_StreamCallback = callback; }
	void SetStreamBufferSize(size_t uRecvBufferSize, size_t uSendBufferSize) override { m_uStreamRecvBufferSize = uRecvBufferSize; m_uStreamSendBufferSize = uSendBufferSize; }
	void Close() override { m_bUserClosed = true; }

	void TryAccept();

	std::function<void(ISocketStream*)> m_StreamCallback;
	size_t m_uStreamRecvBufferSize = 4096;
	size_t m_uStreamSendBufferSize = 4096;

	bool m_bUserClosed = false;
	XSocketManager* m_pSocketMgr = nullptr;
	socket_t m_nSocket = INVALID_SOCKET;
};
