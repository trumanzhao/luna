/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include <memory>
#include <functional>

struct sendv_item
{
	const void* data;
	size_t len;
};

struct socket_mgr
{
    virtual void release() = 0;
    virtual void wait(int timeout) = 0;

    virtual int listen(std::string& err, const char ip[], int port) = 0;
    // 注意: connect总是异步的,需要通过回调函数确认连接成功后,才能发送数据
    virtual int connect(std::string& err, const char domain[], const char service[]) = 0;

    virtual void set_send_cache(uint32_t token, size_t size) = 0;
    virtual void set_recv_cache(uint32_t token, size_t size) = 0;
    virtual void set_timeout(uint32_t token, int duration) = 0; // 设置超时时间,默认-1,即永不超时
    virtual void send(uint32_t token, const void* data, size_t data_len) = 0;
	virtual void sendv(uint32_t token, const sendv_item items[], int count) = 0;
    virtual void close(uint32_t token) = 0;
    virtual bool get_remote_ip(std::string& ip, uint32_t token) = 0;

    virtual void set_accept_callback(uint32_t token, const std::function<void(uint32_t)>& cb) = 0;
    virtual void set_connect_callback(uint32_t token, const std::function<void()>& cb) = 0;
    virtual void set_package_callback(uint32_t token, const std::function<void(char*, size_t)>& cb) = 0;
    virtual void set_error_callback(uint32_t token, const std::function<void(const char*)>& cb) = 0;
};

socket_mgr* create_socket_mgr(int max_fd);
