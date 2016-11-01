/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include "socket_io.h"
#include "luna.h"

// 注意,因为包装层的listener,stream析构的时候,已经close了token
// 所以不存在相关事件(accept, package, error...)触发时,相应的wapper对象失效的问题
// 因为close之后,这些事件不可能触发嘛:)

struct lua_socket_listener
{
	lua_socket_listener(socket_mgr* mgr, int token);
	~lua_socket_listener();

	bool on_accept(int stm);

	socket_mgr* m_mgr = nullptr;
	int m_token = 0;
	DECLARE_LUA_CLASS(lua_socket_listener);
};

//TODO: stream之间需要共享序列化打包器,否则内存开销就非常大咯,嗯,每次都new也不见得不行
struct lua_socket_stream
{
	lua_socket_stream(socket_mgr* mgr, int token);
	~lua_socket_stream();

	int call(lua_State* L);
	bool on_recv(char* msg, size_t msg_len);

	socket_mgr* m_mgr = nullptr;
	int m_token = 0;
	DECLARE_LUA_CLASS(lua_socket_stream);
};

struct lua_socket_mgr
{
	~lua_socket_mgr();
	bool setup(int max_connection);
	socket_mgr* m_mgr = nullptr;
	DECLARE_LUA_CLASS(lua_socket_mgr);
};

lua_socket_mgr* lua_create_socket_mgr(int max_connection);
