/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once
#include <memory>
#include <array>
#include <vector>
#include "socket_io.h"
#include "luna.h"
#include "lua_archiver.h"
#include "io_buffer.h"

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

struct route_mgr
{
	route_mgr(std::shared_ptr<socket_mgr> mgr) { m_mgr = mgr; }

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

struct lua_socket_mgr final
{
public:
    ~lua_socket_mgr();
    bool setup(lua_State* L, int max_fd);
    void wait(int ms) { m_mgr->wait(ms); }
    int listen(lua_State* L);
    int connect(lua_State* L);
    void set_package_size(size_t size); // 设置序列化缓冲区大小,默认64K
    void set_compress_size(size_t size) { m_compress_size = size; }; // 设置启用压缩的阈值,默认UINT_MAX,永不启用
	int route(lua_State* L);

private:
    lua_State* m_lvm = nullptr;
    std::shared_ptr<lua_archiver> m_archiver;
    std::shared_ptr<io_buffer> m_ar_buffer;
    std::shared_ptr<io_buffer> m_lz_buffer;
    std::shared_ptr<socket_mgr> m_mgr;
	std::shared_ptr<route_mgr> m_router;
	size_t m_compress_size = UINT_MAX;

public:
    DECLARE_LUA_CLASS(lua_socket_mgr);
};

// 注意,因为包装层的listener,stream析构的时候,已经close了token
// 所以不存在相关事件(accept, package, error...)触发时,相应的wapper对象失效的问题
// 因为close之后,这些事件不可能触发嘛:)

struct lua_socket_node final
{
    lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr, std::shared_ptr<lua_archiver>& ar,
        std::shared_ptr<io_buffer>& ar_buffer, std::shared_ptr<io_buffer>& lz_buffer, std::shared_ptr<route_mgr> router);

    ~lua_socket_node();

    int call(lua_State* L);
	int forward(lua_State* L);
    void close();
    void set_send_cache(size_t size) { m_mgr->set_send_cache(m_token, size); }
    void set_recv_cache(size_t size) { m_mgr->set_recv_cache(m_token, size); }
    void set_timeout(int duration) { m_mgr->set_timeout(m_token, duration); }

private:
    void on_recv(char* data, size_t data_len);
	void on_call(char* data, size_t data_len);

	uint32_t m_token = 0;
    lua_State* m_lvm = nullptr;
    std::string m_ip;
    std::shared_ptr<socket_mgr> m_mgr;
    std::shared_ptr<lua_archiver> m_archiver;
    std::shared_ptr<io_buffer> m_ar_buffer;
    std::shared_ptr<io_buffer> m_lz_buffer;
	std::shared_ptr<route_mgr> m_router;

public:
    DECLARE_LUA_CLASS(lua_socket_listener);
};

int lua_create_socket_mgr(lua_State* L);
