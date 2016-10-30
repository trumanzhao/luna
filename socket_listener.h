#pragma once

#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"

struct socket_listener : public socket_object
{
	socket_listener();
	~socket_listener() override;
	bool setup(socket_t fd);
	void do_accept(socket_manager* mgr);
	bool update(socket_manager* mgr) override;
	void set_listen_callback(const std::function<void(int64_t)>& cb) override { m_accept_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }

#ifdef _MSC_VER
	void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl);
	void queue_accept(socket_manager* mgr, WSAOVERLAPPED* ovl);
#endif

private:
	socket_t m_listen_socket = INVALID_SOCKET;
	std::function<void(const char*)> m_error_cb;
	std::function<void(int64_t)> m_accept_cb;

#ifdef _MSC_VER
	struct listen_node
	{
		WSAOVERLAPPED ovl;
		socket_t fd;
		sockaddr_storage buffer[2];
	};
	listen_node m_nodes[16];
	LPFN_ACCEPTEX m_accept_func = nullptr;
#endif
};
