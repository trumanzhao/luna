#pragma once

#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"

struct socket_connector : public socket_object
{
	~socket_connector() override;
	bool update(socket_manager* mgr) override;
	void connect(struct addrinfo* addr) override { m_addr = addr; m_next = addr; }
	void on_dns_err(const char* err) override;
	void set_connect_callback(const std::function<void(int64_t)>& cb) override { m_connect_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }
	void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl) override;

	socket_t m_socket = INVALID_SOCKET;
	struct addrinfo* m_addr = nullptr;
	struct addrinfo* m_next = nullptr;
	int64_t m_start_time = get_time_ms();
	int m_timeout = -1;
	std::function<void(const char*)> m_error_cb;
	std::function<void(int64_t)> m_connect_cb;
};

