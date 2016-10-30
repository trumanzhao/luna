#pragma once

#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"

struct socket_stream : public socket_object
{
	socket_stream() {}
	~socket_stream();
	bool setup(socket_t fd);
	bool update(socket_manager*) override { return !m_closed; };
	void set_package_callback(const std::function<void(BYTE*, size_t)>& cb) override { m_package_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }
	void set_send_cache(size_t size) override { m_send_buffer.Resize(size); }
	void set_recv_cache(size_t size) override { m_recv_buffer.Resize(size); }
	void send(const void* data, size_t data_len) override;
	void stream_send(const char* data, size_t data_len);

#ifdef _MSC_VER
	bool queue_send();
	bool queue_recv();
	void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl) override;
#endif

	void do_send();
	void do_recv();

	void dispatch_package();
	void call_error(int err);
	void call_error(const char err[]);

	char m_ip[INET6_ADDRSTRLEN];
	socket_t m_socket = INVALID_SOCKET;
	XSocketBuffer m_recv_buffer;
	XSocketBuffer m_send_buffer;

#ifdef _MSC_VER
	WSAOVERLAPPED m_send_ovl;
	WSAOVERLAPPED m_recv_ovl;
#endif

	std::function<void(BYTE*, size_t)> m_package_cb;
	std::function<void(const char*)> m_error_cb;
};
