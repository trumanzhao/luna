/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2017-02-11, trumanzhao@foxmail.com
*/

#pragma once
#include <memory>
#include <array>
#include <vector>
#include "socket_mgr.h"

enum class msg_id : char
{
    remote_call,
    forward_target,
    forward_master,
    forward_random,
    forward_broadcast,
    forward_hash,
};

const int MAX_SERVICE_GROUP = (UCHAR_MAX + 1);

struct service_node
{
    uint32_t id = 0;
    uint32_t token = 0;
};

struct service_group
{
    uint32_t master = 0;
    std::vector<service_node> nodes;
};

class socket_router
{
public:
    socket_router(socket_mgr& mgr) : m_mgr(mgr){ }

    void update(uint32_t service_id, uint32_t token);
    void set_master(uint8_t group_idx, uint32_t token);
    void erase(uint32_t service_id);
    void forward_target(char* data, size_t data_len);
    void forward_master(char* data, size_t data_len);
    void forward_random(char* data, size_t data_len);
    void forward_broadcast(char* data, size_t data_len);
    void forward_hash(char* data, size_t data_len);

private:
    socket_mgr m_mgr;
    std::array<service_group, MAX_SERVICE_GROUP> m_groups;
};

