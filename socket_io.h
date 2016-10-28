#pragma once

#include <functional>

struct socket_mgr 
{
	virtual void reference() = 0;
	virtual void release() = 0;

	virtual void wait(int timeout) = 0;

	// 注意: connect总是异步的,这里只是返回一个connector,不能用于发送数据,完成后,connector自动关闭
	virtual int64_t connect(std::string& err, const char domain[], const char service[], int timeout) = 0;
	virtual int64_t listen(std::string& err, const char ip[], int port) = 0;

	virtual void set_send_cache(int64_t token, size_t size) = 0;
	virtual void set_recv_cache(int64_t token, size_t size) = 0;
	virtual void send(int64_t token, const void* data, size_t data_len) = 0;
	virtual void close(int64_t token) = 0;
	virtual bool get_remote_ip(std::string& ip, int64_t token) = 0;

	virtual void set_listen_callback(int64_t token, const std::function<void(int64_t)>& cb) = 0;
	virtual void set_connect_callback(int64_t token, const std::function<void(int64_t)>& cb) = 0;
	virtual void set_package_callback(int64_t token, const std::function<void(BYTE*, size_t)>& cb) = 0;
	virtual void set_error_callback(int64_t token, const std::function<void(const char*)>& cb) = 0;
};

socket_mgr* create_socket_mgr(int max_fd);