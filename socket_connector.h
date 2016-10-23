#pragma once

#include <thread>
#include "socket_helper.h"
#include "socket_mgr.h"

class xconnector_t : public connector_t
{
public:
	xconnector_t(XSocketManager* mgr, const char node[], const char service[]);
	~xconnector_t();

	void on_connect(const std::function<void(ISocketStream*)>& cb) override { m_connect_cb = cb; }
	void on_error(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }
	void close() override { m_user_closed = true; }
	bool update(); // 供mgr调用,返回false表示已经结束可以删掉了

private:
	void work();
	void do_connect();

	std::string m_node;
	std::string m_service;
	size_t m_send_buffer_size = 1024 * 16;
	size_t m_recv_buffer_size = 1024 * 16;
	std::function<void(ISocketStream*)> m_connect_cb;
	std::function<void(const char* err)> m_error_cb;
	bool m_callbacked = false;
	// 某些情景下,还是有必要大量发起连接的,所以,每次连接都开启一个线程还是不大合适:)
	std::thread m_thread;
	volatile bool m_user_closed = false;
	volatile bool m_dns_resolved = false;
	struct addrinfo* m_addr = nullptr;
	struct addrinfo* m_next = nullptr;
	socket_t m_socket = INVALID_SOCKET;
	std::string m_dns_error;
	XSocketManager* m_mgr = nullptr;
	int64_t m_start_time = 0;
	int m_timeout = -1;
};
