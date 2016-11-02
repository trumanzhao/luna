/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include "socket_io.h"
#include "luna.h"
#include "lua_archiver.h"

// 注意,因为包装层的listener,stream析构的时候,已经close了token
// 所以不存在相关事件(accept, package, error...)触发时,相应的wapper对象失效的问题
// 因为close之后,这些事件不可能触发嘛:)

struct lua_socket_listener
{
	lua_socket_listener(lua_State* L, socket_mgr* mgr, lua_archiver* archiver, int token);
	~lua_socket_listener();

private:
	socket_mgr* m_mgr = nullptr;
	lua_archiver* m_archiver = nullptr;
	lua_State* m_lvm = nullptr;
	int m_token = 0;

public:
	DECLARE_LUA_CLASS(lua_socket_listener);
};

//TODO: stream之间需要共享序列化打包器,否则内存开销就非常大咯,嗯,每次都new也不见得不行
// lua_archiver不可能同时多个都在使用中,所以,全局共享一个就够了,大小取所需的最大值
struct lua_socket_stream
{
	lua_socket_stream(lua_State* L, socket_mgr* mgr, lua_archiver* archiver, int token);
	~lua_socket_stream();

	size_t call(lua_State* L);

private:
	void on_remote_call(char* data, size_t data_len);

private:
	socket_mgr* m_mgr = nullptr;
	lua_archiver* m_archiver = nullptr;
	lua_State* m_lvm = nullptr;
	int m_token = 0;

public:
	DECLARE_LUA_CLASS(lua_socket_stream);
};

struct lua_socket_mgr
{
public:
	~lua_socket_mgr();
	bool setup(lua_State* L, int max_fd, size_t buffer_size, size_t compress_threhold);
	void wait(int ms);
	int listen(lua_State* L);
private:
	lua_State* m_lvm = nullptr;
	socket_mgr* m_mgr = nullptr;
	lua_archiver* m_archiver = nullptr;

public:
	DECLARE_LUA_CLASS(lua_socket_mgr);
};

int lua_create_socket_mgr(lua_State* L);
