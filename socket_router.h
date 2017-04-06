/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2017-02-11, trumanzhao@foxmail.com
*/

#pragma once
#include <memory>
#include <array>
#include <vector>
#include "socket_io.h"

enum class msg_id : char
{
    remote_call,
    forward_target,
    forward_master,
    forward_random,
    forward_broadcast,
    forward_hash,
};

const int MAX_SERVICE_CLASS = UCHAR_MAX + 1;

struct service_node
{
    uint32_t id = 0;
    uint32_t token = 0;
};

struct service_class
{
    service_node master;
    std::vector<service_node> nodes;
};

template <typename T>
static inline bool read_var(T& to, char*& data, size_t& data_len)
{
    static_assert(std::is_enum<T>::value == false || sizeof(T) == 1, "sizeof(enum T) should == 1 !");
    if (data_len < sizeof(to))
        return false;
    memcpy(&to, data, sizeof(to));
    data += sizeof(to);
    data_len -= sizeof(to);
    return true;
}

template <typename T>
static inline bool write_var(BYTE*& buffer, size_t& buffer_size, const T& v)
{
    static_assert(std::is_enum<T>::value == false || sizeof(T) == 1, "sizeof(enum T) should == 1 !");
    if (buffer_size < sizeof(v))
        return false;
    memcpy(buffer, &v, sizeof(v));
    buffer += sizeof(v);
    buffer_size -= sizeof(v);
    return true;
}

struct socket_router
{
    socket_router(std::shared_ptr<socket_mgr> mgr) { m_mgr = mgr; }

    void update(uint32_t service_id, uint32_t token);
    void erase(uint32_t service_id);
    void forward_target(char* data, size_t data_len);
    void forward_master(char* data, size_t data_len);
    void forward_random(char* data, size_t data_len);
    void forward_broadcast(char* data, size_t data_len);
    void forward_hash(char* data, size_t data_len);

    std::shared_ptr<socket_mgr> m_mgr;
    std::array<service_class, MAX_SERVICE_CLASS> m_routes;
};

