/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include <limits.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "socket_helper.h"
#include "socket_mgr.h"

struct socket_object
{
    virtual ~socket_object() {};
    virtual bool update(int64_t now) = 0;
    virtual void close() final { m_closed = true; };
    virtual bool get_remote_ip(std::string& ip) = 0;
    virtual void connect(const char node_name[], const char service_name[]) { }
    virtual void set_send_cache(size_t size) { }
    virtual void set_recv_cache(size_t size) { }
    virtual void set_timeout(int duration) { }
    virtual void send(const void* data, size_t data_len) { }
    virtual void sendv(const sendv_item items[], int count) { };
    virtual void set_accept_callback(const std::function<void(int)>& cb) { }
    virtual void set_connect_callback(const std::function<void()>& cb) { }
    virtual void set_package_callback(const std::function<void(char*, size_t)>& cb) { }
    virtual void set_error_callback(const std::function<void(const char*)>& cb) { }

#ifdef _MSC_VER
    virtual void on_complete(WSAOVERLAPPED* ovl) = 0;
#endif

#if defined(__linux) || defined(__APPLE__)
    virtual void on_can_recv(size_t data_len = UINT_MAX, bool is_eof = false) {};
    virtual void on_can_send(size_t data_len = UINT_MAX, bool is_eof = false) {};
#endif

protected:
    bool m_closed = false;
};

class socket_mgr_impl
{
public:
    socket_mgr_impl();
    ~socket_mgr_impl();

    bool setup(int max_connection);

#ifdef _MSC_VER
    bool get_socket_funcs();
#endif

    void wait(int timout);

    int listen(std::string& err, const char ip[], int port);
    int connect(std::string& err, const char node_name[], const char service_name[]);

    void set_send_cache(uint32_t token, size_t size);
    void set_recv_cache(uint32_t token, size_t size);
    void set_timeout(uint32_t token, int duration);
    void send(uint32_t token, const void* data, size_t data_len);
    void sendv(uint32_t token, const sendv_item items[], int count);
    void close(uint32_t token);
    bool get_remote_ip(uint32_t token, std::string& ip);

    void set_accept_callback(uint32_t token, const std::function<void(uint32_t)>& cb);
    void set_connect_callback(uint32_t token, const std::function<void()>& cb);
    void set_package_callback(uint32_t token, const std::function<void(char*, size_t)>& cb);
    void set_error_callback(uint32_t token, const std::function<void(const char*)>& cb);

    bool watch_listen(socket_t fd, socket_object* object);
    bool watch_accepted(socket_t fd, socket_object* object);
    bool watch_connecting(socket_t fd, socket_object* object);
    bool watch_connected(socket_t fd, socket_object* object);
    bool watch_send(socket_t fd, socket_object* object, bool enable);
    int accept_stream(socket_t fd, const char ip[]);

    void increase_count() { m_count++; }
    void decrease_count() { m_count--; }
    bool is_full() { return m_count >= m_max_count; }
private:

#ifdef _MSC_VER
    LPFN_ACCEPTEX m_accept_func = nullptr;
    LPFN_CONNECTEX m_connect_func = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS m_addrs_func = nullptr;
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

    uint32_t new_token()
    {
        while (m_token == 0 || m_objects.find(m_token) != m_objects.end())
        {
            ++m_token;
        }
        return m_token++;
    }

    int m_max_count = 0;
    int m_count = 0;
    uint32_t m_token = 0;
    int64_t m_next_update = 0;
    std::unordered_map<uint32_t, socket_object*> m_objects;
};
