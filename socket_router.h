/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2017-02-11, trumanzhao@foxmail.com
*/

#pragma once
#include <memory>
#include <array>
#include <vector>
#include "socket_io.h"

enum class msg_id
{
	remote_call,
	forward_target,
	forward_master,
	forward_hash,
	forward_random,
	forward_broadcast,
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

struct socket_router
{
    socket_router(std::shared_ptr<socket_mgr> mgr) { m_mgr = mgr; }

    void update(uint32_t service_id, uint32_t token);
    void erase(uint32_t service_id);
    void forward_target(char* data, size_t data_len);
    void forward_random(char* data, size_t data_len);
    void forward_master(char* data, size_t data_len);
    void forward_hash(char* data, size_t data_len);
    void forward_broadcast(char* data, size_t data_len);

    std::shared_ptr<socket_mgr> m_mgr;
    std::array<service_class, MAX_SERVICE_CLASS> m_routes;
};

