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

class socket_mgr
{
public:
    socket_mgr();
    ~socket_mgr();

    bool setup(int max_connection);
    void wait(int timeout);
    int listen(std::string& err, const char ip[], int port);
    // 注意: connect总是异步的,需要通过回调函数确认连接成功后,才能发送数据
    int connect(std::string& err, const char node_name[], const char service_name[]);

    void set_send_cache(uint32_t token, size_t size);
    void set_recv_cache(uint32_t token, size_t size);
    void set_timeout(uint32_t token, int duration); // 设置超时时间,默认-1,即永不超时
    void send(uint32_t token, const void* data, size_t data_len);
    void sendv(uint32_t token, const sendv_item items[], int count);
    void close(uint32_t token);
    bool get_remote_ip(uint32_t token, std::string& ip);

    void set_accept_callback(uint32_t token, const std::function<void(uint32_t)>& cb);
    void set_connect_callback(uint32_t token, const std::function<void()>& cb);
    void set_package_callback(uint32_t token, const std::function<void(char*, size_t)>& cb);
    void set_error_callback(uint32_t token, const std::function<void(const char*)>& cb);
private:
    std::shared_ptr<class socket_mgr_impl> m_impl;
};
