/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2017-02-11, trumanzhao@foxmail.com
*/

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <algorithm>
#include "tools.h"
#include "var_int.h"
#include "socket_router.h"

uint32_t get_group_idx(uint32_t service_id) { return  (service_id >> 16) & 0xff; }

void socket_router::update(uint32_t service_id, uint32_t token)
{
    uint32_t group_idx = get_group_idx(service_id);
    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    auto it = std::lower_bound(nodes.begin(), nodes.end(), service_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it != nodes.end() && it->id == service_id)
    {
        it->token = token;
    }
    else
    {
        service_node node;
        node.id = service_id;
        node.token = token;
        nodes.insert(it, node);
    }
}

void socket_router::set_master(uint8_t group_idx, uint32_t token)
{
    auto& group = m_groups[group_idx];
    group.master = token;
}

void socket_router::erase(uint32_t service_id)
{
    uint32_t group_idx = get_group_idx(service_id);
    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    auto it = std::lower_bound(nodes.begin(), nodes.end(), service_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it != nodes.end() && it->id == service_id)
    {
        nodes.erase(it);
    }
}

void socket_router::forward_target(char* data, size_t data_len)
{
    uint64_t target_id64 = 0;
    size_t len = decode_u64(&target_id64, (BYTE*)data, data_len);
    if (len == 0)
        return;
    data += len;
    data_len -= len;

    uint32_t target_id = (uint32_t)target_id64;
    uint32_t group_idx = get_group_idx(target_id);
    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    auto it = std::lower_bound(nodes.begin(), nodes.end(), target_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it == nodes.end() || it->id != target_id)
        return;

    BYTE msg_id_data[MAX_ENCODE_LEN];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);

    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};
    m_mgr.sendv(it->token, items, _countof(items));
}

void socket_router::forward_master(char* data, size_t data_len)
{
    uint64_t group_idx = 0;
    size_t len = decode_u64(&group_idx, (BYTE*)data, data_len);
    if (len == 0 || group_idx >= m_groups.size())
        return;
    data += len;
    data_len -= len;

    auto token = m_groups[group_idx].master;
    if (token == 0)
        return;

    BYTE msg_id_data[MAX_ENCODE_LEN];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);

    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};
    m_mgr.sendv(token, items, _countof(items));
}

void socket_router::forward_random(char* data, size_t data_len)
{
    uint64_t group_idx = 0;
    size_t len = decode_u64(&group_idx, (BYTE*)data, data_len);
    if (len == 0 || group_idx >= m_groups.size())
        return;
    data += len;
    data_len -= len;

    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    int count = (int)nodes.size();
    if (count == 0)
        return;

    BYTE msg_id_data[MAX_ENCODE_LEN];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);
    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};

    int idx = rand() % count;
    for (int i = 0; i < count; i++)
    {
        auto& target = nodes[(idx + i) % count];
        if (target.token != 0)
        {
            m_mgr.sendv(target.token, items, _countof(items));
            break;
        }
    }
}

void socket_router::forward_broadcast(char* data, size_t data_len)
{
    uint64_t group_idx = 0;
    size_t len = decode_u64(&group_idx, (BYTE*)data, data_len);
    if (len == 0 || group_idx >= m_groups.size())
        return;
    data += len;
    data_len -= len;

    BYTE msg_id_data[MAX_ENCODE_LEN];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);
    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};

    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    int count = (int)nodes.size();
    for (auto& target : nodes)
    {
        if (target.token != 0)
        {
            m_mgr.sendv(target.token, items, _countof(items));
        }
    }
}

void socket_router::forward_hash(char* data, size_t data_len)
{
    uint64_t group_idx = 0;
    size_t len = decode_u64(&group_idx, (BYTE*)data, data_len);
    if (len == 0 || group_idx >= m_groups.size())
        return;
    data += len;
    data_len -= len;

    uint64_t hash = 0;
    len = decode_u64(&hash, (BYTE*)data, data_len);
    if (len == 0)
        return;
    data += len;
    data_len -= len;

    auto& group = m_groups[group_idx];
    auto& nodes = group.nodes;
    int count = (int)nodes.size();
    if (count == 0)
        return;

    BYTE msg_id_data[MAX_ENCODE_LEN];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);
    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};

    int idx = hash % count;
    for (int i = 0; i < count; i++)
    {
        auto& target = nodes[(idx + i) % count];
        if (target.token != 0)
        {
            m_mgr.sendv(target.token, items, _countof(items));
            break;
        }
    }
}
