#pragma once

#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include "socket_helper.h"
#include "socket_io.h"
#include "dns_resolver.h"

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
	virtual void set_listen_callback(const std::function<void(int)>& cb) { assert(!"not supported"); }
	virtual void set_connect_callback(const std::function<void()>& cb) { assert(!"not supported"); }
	virtual void set_package_callback(const std::function<void(char*, size_t)>& cb) { assert(!"not supported"); }
	virtual void set_error_callback(const std::function<void(const char*)>& cb) { assert(!"not supported"); }

#ifdef _MSC_VER
	virtual void on_complete(socket_manager* mgr, WSAOVERLAPPED* ovl) = 0;
#endif

#if defined(__linux) || defined(__APPLE__)
    virtual void on_complete(socket_manager* mgr, bool can_read, bool can_write) = 0;
#endif

protected:
	bool m_closed = false;
};

struct socket_manager : socket_mgr
{
	socket_manager();
	~socket_manager();

	bool setup(int max_connection);

	virtual void reference() override { ++m_ref; }
	virtual void release() override { if (--m_ref == 0) delete this; }

	virtual void wait(int timout) override;

	virtual int listen(std::string& err, const char ip[], int port) override;
	virtual int connect(std::string& err, const char domain[], const char service[]) override;

	virtual void set_send_cache(int token, size_t size) override;
	virtual void set_recv_cache(int token, size_t size) override;
	virtual void send(int token, const void* data, size_t data_len) override;
	virtual void close(int token) override;
	virtual bool get_remote_ip(std::string& ip, int token) override;

	virtual void set_listen_callback(int token, const std::function<void(int)>& cb) override;
	virtual void set_connect_callback(int token, const std::function<void()>& cb) override;
	virtual void set_package_callback(int token, const std::function<void(char*, size_t)>& cb) override;
	virtual void set_error_callback(int token, const std::function<void(const char*)>& cb) override;

	bool watch(socket_t fd, socket_object* object, bool watch_recv, bool watch_send, bool modify = false);
	int accept_stream(socket_t fd);

private:

#ifdef _MSC_VER
	HANDLE m_handle = INVALID_HANDLE_VALUE;
	std::vector<OVERLAPPED_ENTRY> m_events;
#endif

#ifdef __linux
	int m_handle = -1;
	std::vector<epoll_event> m_events;
#endif

#ifdef __APPLE__
	int m_handle = -1;
	std::vector<struct kevent> m_events;
#endif

	socket_object* get_object(int token)
	{
		auto it = m_objects.find(token);
		if (it != m_objects.end())
		{
			return it->second;
		}
		return nullptr;
	}

	int new_token()
	{
		while (m_token == 0 || m_objects.find(m_token) != m_objects.end())
		{
			++m_token;
		}
		return m_token;
	}

	int m_max_connection = 0;
	int m_ref = 1;
	int m_token = 0;
	std::unordered_map<int, socket_object*> m_objects;
	dns_resolver m_dns;
};
