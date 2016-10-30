#pragma once

#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>
#include "socket_helper.h"
#include "socket_io.h"
#include "dns_resolver.h"

//todo: IOCP,一个请求为完成前,关闭socket,那么还会出发事件么?
//todo: mac keuque处理

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

struct socket_manager : socket_mgr
{
	socket_manager();
	~socket_manager();

	bool setup(int max_connection);

	virtual void reference() override { ++m_ref; }
	virtual void release() override { if (--m_ref == 0) delete this; }

	virtual void wait(int timout) override;

	virtual int64_t listen(std::string& err, const char ip[], int port) override;
	virtual int64_t connect(std::string& err, const char domain[], const char service[], int timeout) override;

	virtual void set_send_cache(int64_t token, size_t size) override;
	virtual void set_recv_cache(int64_t token, size_t size) override;
	virtual void send(int64_t token, const void* data, size_t data_len) override;
	virtual void close(int64_t token) override;
	virtual bool get_remote_ip(std::string& ip, int64_t token) override;

	virtual void set_listen_callback(int64_t token, const std::function<void(int64_t)>& cb) override;
	virtual void set_connect_callback(int64_t token, const std::function<void(int64_t)>& cb) override;
	virtual void set_package_callback(int64_t token, const std::function<void(BYTE*, size_t)>& cb) override;
	virtual void set_error_callback(int64_t token, const std::function<void(const char*)>& cb) override;

	bool watch(socket_t fd, socket_object* object, bool watch_write);
	int64_t new_stream(socket_t fd);

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

	socket_object* get_object(int64_t token)
	{
		auto it = m_objects.find(token);
		if (it != m_objects.end())
		{
			return it->second;
		}
		return nullptr;
	}

	int64_t new_token()
	{
		while (m_token == 0 || m_objects.find(m_token) != m_objects.end())
		{
			++m_token;
		}
		return m_token;
	}

	int m_max_connection = 0;
	int m_ref = 1;
	int64_t m_token = 0;
	std::unordered_map<int64_t, socket_object*> m_objects;
	dns_resolver m_dns;
};
