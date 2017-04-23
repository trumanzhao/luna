/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr_impl.h"

struct socket_stream : public socket_object
{
#ifdef _MSC_VER
    socket_stream(socket_mgr_impl* mgr, LPFN_CONNECTEX connect_func);
#endif
    socket_stream(socket_mgr_impl* mgr);

    ~socket_stream();
    bool get_remote_ip(std::string& ip) override;
    bool accept_socket(socket_t fd, const char ip[]);
    void connect(const char node_name[], const char service_name[]) override { m_node_name = node_name; m_service_name = service_name; };
    bool update(int64_t now) override;
    bool do_connect();
    void try_connect();
    void set_package_callback(const std::function<void(char*, size_t)>& cb) override { m_package_cb = cb; }
    void set_error_callback(const std::function<void(const char*)>& cb) override { m_error_cb = cb; }
    void set_connect_callback(const std::function<void()>& cb) override { m_connect_cb = cb; }
    void set_send_cache(size_t size) override { m_send_buffer->resize(size); }
    void set_recv_cache(size_t size) override { m_recv_buffer->resize(size); }
    void set_timeout(int duration) override { m_timeout = duration; }
    void send(const void* data, size_t data_len) override;
    void sendv(const sendv_item items[], int count) override;
    void stream_send(const char* data, size_t data_len);

#ifdef _MSC_VER
    void on_complete(WSAOVERLAPPED* ovl) override;
#endif

#if defined(__linux) || defined(__APPLE__)
    void on_can_recv(size_t max_len, bool is_eof) override { do_recv(max_len, is_eof); }
    void on_can_send(size_t max_len, bool is_eof) override;
#endif

    void do_send(size_t max_len, bool is_eof);
    void do_recv(size_t max_len, bool is_eof);

    void dispatch_package();
    void call_error(const char err[]);

    socket_mgr_impl* m_mgr = nullptr;
    socket_t m_socket = INVALID_SOCKET;
    std::unique_ptr<io_buffer> m_recv_buffer = std::make_unique<io_buffer>();
    std::unique_ptr<io_buffer> m_send_buffer = std::make_unique<io_buffer>();

    std::string m_node_name;
    std::string m_service_name;
    struct addrinfo* m_addr = nullptr;
    struct addrinfo* m_next = nullptr;
    char m_ip[INET6_ADDRSTRLEN];
    bool m_connected = false;
    int m_timeout = -1;
    int64_t m_alive_time = get_time_ms();

#ifdef _MSC_VER
    LPFN_CONNECTEX m_connect_func = nullptr;
    WSAOVERLAPPED m_send_ovl;
    WSAOVERLAPPED m_recv_ovl;
    int m_ovl_ref = 0;
#endif

    std::function<void(char*, size_t)> m_package_cb;
    std::function<void(const char*)> m_error_cb;
    std::function<void()> m_connect_cb;
};
