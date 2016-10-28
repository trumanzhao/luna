#pragma once

#include "socket_helper.h"
#include "io_buffer.h"

struct socket_manager;

struct socket_object
{
	virtual ~socket_object() {};
	virtual bool update(socket_manager* mgr) = 0;
	virtual void close() { m_closed = true; };
	virtual void connect(struct addrinfo* addr) { assert(!"not implemented"); }
	virtual void on_dns_err(const char* err) { assert(!"not implemented"); }
	virtual void set_send_cache(size_t size) { assert(!"not implemented"); }
	virtual void set_recv_cache(size_t size) { assert(!"not implemented"); }
	virtual void send(const void* data, size_t data_len) { assert(!"not implemented"); }
	virtual void set_listen_callback(const std::function<void(int64_t)>& cb) { assert(!"not implemented"); }
	virtual void set_connect_callback(const std::function<void(int64_t)>& cb) { assert(!"not implemented"); }
	virtual void set_package_callback(const std::function<void(BYTE*, size_t)>& cb) { assert(!"not implemented"); }
	virtual void set_error_callback(const std::function<void(const char*)>& cb) { assert(!"not implemented"); }

protected:
	bool m_closed = false;
};

struct socket_connector : public socket_object
{
	~socket_connector() override;
	bool update(socket_manager* mgr) override;
	void connect(struct addrinfo* addr) override { m_addr = addr; m_next = addr; }
	void on_dns_err(const char* err) override;
	void set_connect_callback(const std::function<void(int64_t)>& cb) override { m_connect_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }

	socket_t m_socket = INVALID_SOCKET;
	struct addrinfo* m_addr = nullptr;
	struct addrinfo* m_next = nullptr;
	int64_t m_start_time = get_time_ms();
	int m_timeout = -1;
	std::function<void(const char*)> m_error_cb;
	std::function<void(int64_t)> m_connect_cb;
};

struct socket_listener : public socket_object
{
	socket_listener(socket_t fd) : m_socket(fd) {};
	~socket_listener() override;
	bool update(socket_manager* mgr) override;
	void set_listen_callback(const std::function<void(int64_t)>& cb) override { m_accept_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }

	socket_t m_socket = INVALID_SOCKET;
	std::function<void(const char*)> m_error_cb;
	std::function<void(int64_t)> m_accept_cb;
};

struct socket_stream : public socket_object
{
	socket_stream(socket_t fd);
	~socket_stream() override;

	bool update(socket_manager*) override;
	void set_package_callback(const std::function<void(BYTE*, size_t)>& cb) override { m_package_cb = cb; }
	void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }
	void set_send_cache(size_t size) override { m_SendBuffer.Resize(size); }
	void set_recv_cache(size_t size) override { m_RecvBuffer.Resize(size); }
	void send(const void* data, size_t data_len) override;

	void StreamSend(const void* pvData, size_t uDataLen);

#ifdef _MSC_VER
	void AsyncSend();
	void AsyncRecv();
	void OnComplete(WSAOVERLAPPED* pOVL, DWORD dwLen);
	void OnRecvComplete(size_t uLen);
	void OnSendComplete(size_t uLen);
#endif

#if defined(__linux) || defined(__APPLE__)
	void OnSendAble();
	void OnRecvAble();
#endif

	void DispatchPackage();
	void call_error(int err);
	void call_error(const char err[]);

	char m_ip[INET6_ADDRSTRLEN];
	socket_t m_socket = INVALID_SOCKET;
	XSocketBuffer m_RecvBuffer;
	XSocketBuffer m_SendBuffer;

#ifdef _MSC_VER
	DWORD m_dwCompletionKey = 0;
	WSAOVERLAPPED m_wsSendOVL;
	WSAOVERLAPPED m_wsRecvOVL;
	bool m_send_complete = true;
	bool m_recv_complete = true;
#endif

#if defined(__linux) || defined(__APPLE__)
	bool m_bWriteAble = true;
#endif

	std::function<void(BYTE*, size_t)> m_package_cb;
	std::function<void(const char*)> m_error_cb;
};
