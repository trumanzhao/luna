#pragma once

#include "socket_helper.h"
#include "socket_mgr.h"

struct XSocketManager;

struct XSocketListener : ISocketListener
{
	XSocketListener(XSocketManager* pMgr, socket_t nSocket);
	~XSocketListener();

	void SetStreamCallback(const std::function<void(ISocketStream* pSocket)>& callback) override { m_StreamCallback = callback; }
	void SetStreamBufferSize(size_t uRecvBufferSize, size_t uSendBufferSize) override { m_recv_buffer_size = uRecvBufferSize; m_send_buffer_size = uSendBufferSize; }
	void Close() override { m_user_closed = true; }

	void TryAccept();

	std::function<void(ISocketStream*)> m_StreamCallback;
	size_t m_recv_buffer_size = 4096;
	size_t m_send_buffer_size = 4096;

	bool m_user_closed = false;
	XSocketManager* m_pSocketMgr = nullptr;
	socket_t m_socket = INVALID_SOCKET;
};
