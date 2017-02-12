/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2017-02-11, trumanzhao@foxmail.com
*/

#include "tools.h"
#include "socket_router.h"

uint32_t get_service_class(uint32_t service_id) { return  (service_id & 0xff000000) >> 24; }
uint32_t get_service_instance(uint32_t service_id) { return  service_id & 0x00ffffff; }

static void update_master(service_class& class_tab)
{
    class_tab.master = service_node();
    for (auto& node : class_tab.nodes)
    {
        if (node.token != 0)
        {
            class_tab.master = node;
            return;
        }
    }
}

void socket_router::update(uint32_t service_id, uint32_t token)
{
    if (service_id == 0)
        return;

    uint32_t class_idx = get_service_class(service_id);
    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    auto it = std::lower_bound(class_nodes.begin(), class_nodes.end(), service_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it != class_nodes.end() && it->id == service_id)
    {
        it->token = token;
    }
    else
    {
        service_node node;
        node.id = service_id;
        node.token = token;
        class_nodes.insert(it, node);
    }

    if (class_tab.master.id == 0 || class_tab.master.id == service_id)
    {
        update_master(class_tab);
    }
}

void socket_router::erase(uint32_t service_id)
{
    uint32_t class_idx = get_service_class(service_id);
    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    auto it = std::lower_bound(class_nodes.begin(), class_nodes.end(), service_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it != class_nodes.end() && it->id == service_id)
    {
        class_nodes.erase(it);
        if (class_tab.master.id == service_id)
        {
            update_master(class_tab);
        }
    }
}

template <typename T>
static inline bool read_var(T& to, char*& data, size_t& data_len)
{
    if (data_len < sizeof(to))
        return false;
    memcpy(&to, data, sizeof(to));
    data += sizeof(to);
    data_len -= sizeof(to);
    return true;
}

static inline void set_call_header(char*& data, size_t& data_len)
{	
	data--;
	data_len++;
	*data = (char)msg_id::remote_call;
}

void socket_router::forward_target(char* data, size_t data_len)
{
    uint32_t target_id = 0;
    if (!read_var(target_id, data, data_len) || data_len == 0)
        return;

	set_call_header(data, data_len);

    uint32_t class_idx = get_service_class(target_id);
    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    auto it = std::lower_bound(class_nodes.begin(), class_nodes.end(), target_id, [](auto& node, uint32_t id) { return node.id < id; });
    if (it == class_nodes.end() || it->id != target_id)
        return;

    m_mgr->send(it->token, data, data_len);
}

void socket_router::forward_random(char* data, size_t data_len)
{
    uint8_t class_idx = 0;
    if (!read_var(class_idx, data, data_len) || data_len == 0)
        return;

	set_call_header(data, data_len);

    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    int count = (int)class_nodes.size();
    if (count == 0)
        return;

    int idx = std::rand() % count;
    auto& target = class_nodes[idx];
    if (target.token != 0)
    {
        m_mgr->send(target.token, data, data_len);
    }
}

void socket_router::forward_master(char* data, size_t data_len)
{
    uint8_t class_idx = 0;
    if (!read_var(class_idx, data, data_len) || data_len == 0)
        return;

	set_call_header(data, data_len);

    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    for (auto& target : class_nodes)
    {
        if (target.token != 0)
        {
            m_mgr->send(target.token, data, data_len);
            break;
        }
    }
}

void socket_router::forward_hash(char* data, size_t data_len)
{
    uint8_t class_idx = 0;
    uint32_t hash = 0;
    if (!read_var(class_idx, data, data_len) || !read_var(hash, data, data_len) || data_len == 0)
        return;

	set_call_header(data, data_len);

    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    int count = (int)class_nodes.size();
    if (count == 0)
        return;

    int idx = hash % count;
    for (int i = 0; i < count; i++)
    {
        auto& target = class_nodes[(idx + i) % count];
        if (target.token != 0)
        {
            m_mgr->send(target.token, data, data_len);
            break;
        }
    }
}

void socket_router::forward_broadcast(char* data, size_t data_len)
{
    uint8_t class_idx = 0;
    if (!read_var(class_idx, data, data_len) || data_len == 0)
        return;

	set_call_header(data, data_len);

    auto& class_tab = m_routes[class_idx];
    auto& class_nodes = class_tab.nodes;
    int count = (int)class_nodes.size();
    for (auto& target : class_nodes)
    {
        if (target.token != 0)
        {
            m_mgr->send(target.token, data, data_len);
        }
    }
}
