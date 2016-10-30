#pragma once

#include "socket_helper.h"
#include "io_buffer.h"

struct socket_manager;

struct socket_object
{
	virtual ~socket_object() {};
	virtual bool update(socket_manager* mgr) = 0;
	virtual void close() { m_closed = true; };
	virtual void connect(struct addrinfo* addr) { assert(!"not supported"); }
	virtual void on_dns_err(const char* err) { assert(!"not supported"); }
	virtual void set_send_cache(size_t size) { assert(!"not supported"); }
	virtual void set_recv_cache(size_t size) { assert(!"not supported"); }
	virtual void send(const void* data, size_t data_len) { assert(!"not supported"); }
	virtual void set_listen_callback(const std::function<void(int64_t)>& cb) { assert(!"not supported"); }
	virtual void set_connect_callback(const std::function<void(int64_t)>& cb) { assert(!"not supported"); }
	virtual void set_package_callback(const std::function<void(BYTE*, size_t)>& cb) { assert(!"not supported"); }
	virtual void set_error_callback(const std::function<void(const char*)>& cb) { assert(!"not supported"); }

#ifdef _MSC_VER
	virtual void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl) = 0;
#endif

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
	void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl) override;

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
