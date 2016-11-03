/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/
#include "tools.h"
#include "socket_wapper.h"
#include "lua/lstate.h"

EXPORT_CLASS_BEGIN(lua_socket_mgr)
EXPORT_LUA_FUNCTION(wait)
EXPORT_LUA_FUNCTION(listen)
EXPORT_CLASS_END()

lua_socket_mgr::~lua_socket_mgr()
{
}

bool lua_socket_mgr::setup(lua_State* L, int max_fd, size_t buffer_size, size_t compress_threhold)
{
	auto mgr = create_socket_mgr(max_fd);
	m_tool = std::make_shared<lua_socket_tool>();
	m_tool->lvm = L;
	m_tool->mgr = std::shared_ptr<socket_mgr>(mgr, [](auto o) { o->release(); });
	return mgr != nullptr;
}

void lua_socket_mgr::wait(int ms)
{
	m_tool->mgr->wait(ms);
}

int lua_socket_mgr::listen(lua_State* L)
{
	const char* ip = lua_tostring(L, 1);
	int port = (int)lua_tointeger(L, 2);
	if (ip == nullptr || port <= 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, "invalid param");
		return 2;
	}

	std::string err;
	int token = m_tool->mgr->listen(err, ip, port);
	if (token == 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, err.c_str());
		return 2;
	}

	auto listener = new lua_socket_listener(token, m_tool);
	lua_push_object(L, listener);
	lua_pushstring(L, "OK");
	return 2;
}

EXPORT_CLASS_BEGIN(lua_socket_listener)
EXPORT_CLASS_END()

lua_socket_listener::lua_socket_listener(int token, std::shared_ptr<lua_socket_tool> tool)
{
	m_token = token;
	m_tool = tool;

	m_tool->mgr->set_accept_callback(token, [this](int steam_token)
	{
		auto stream = new lua_socket_stream(steam_token, m_tool);
		if (!lua_call_object_function(m_tool->lvm, this, "on_accept", std::tie(), stream))
			delete stream;
	});

	m_tool->mgr->set_error_callback(token, [this](const char* err)
	{
		lua_call_object_function(m_tool->lvm, this, "on_error", std::tie(), err);
	});
}

lua_socket_listener::~lua_socket_listener()
{
	m_tool->mgr->close(m_token);
}

EXPORT_CLASS_BEGIN(lua_socket_stream)
EXPORT_LUA_FUNCTION(call)
EXPORT_CLASS_END()

lua_socket_stream::lua_socket_stream(int token, std::shared_ptr<lua_socket_tool> tool)
{
	m_token = token;
	m_tool = tool;

	m_tool->mgr->set_package_callback(token, [this](char* data, size_t data_len)
	{
		on_remote_call(data, data_len);
	});

	m_tool->mgr->set_error_callback(token, [this](const char* err)
	{
		lua_call_object_function(m_tool->lvm, this, "on_error", std::tie(), err);
	});
}

lua_socket_stream::~lua_socket_stream()
{
	m_tool->mgr->close(m_token);
}

size_t lua_socket_stream::call(lua_State* L)
{
	int top = lua_gettop(L);
	if (top < 1 || lua_type(L, 1) != LUA_TSTRING)
		return 0;

	const char* function = lua_tostring(L, 1);

	// 如果archieve的时候,一般都不需要压缩的话,那么最高效的方式应该是save到外部缓冲区,这样就少一次copy
	// 但是,这样的话,外面就得维护一个缓冲区....
	// 而且,如果触发压缩呢? 嗯,所以,外部得再提供一个压缩缓冲区,并额外再调用一次压缩?
	//m_archiver->save();

	return 0;
}

void lua_socket_stream::on_remote_call(char* data, size_t data_len)
{
	char* data_end = data;
	char* name_end = data;

	while (*name_end != '\0' && name_end < data_end)
		name_end++;

	if (name_end >= data_end)
		return;

	name_end++;

	char* function = data;
	char* param = name_end;
	size_t param_len = data_end - name_end;

	lua_guard_t g(m_tool->lvm);

	if (!lua_get_object_function(m_tool->lvm, this, "on_recv"))
		return;

	//lua_pushstring(m_lvm, function);
	//int param_count = m_archiver->load(m_lvm, (BYTE*)data, data_len);
	//lua_call_function(m_lvm, 1 + param_count, 0);
}

int lua_create_socket_mgr(lua_State* L)
{
	int top = lua_gettop(L);
	if (top != 3)
	{
		lua_pushnil(L);
		return 1;
	}

	int max_fd = (int)lua_tointeger(L, 1);
	size_t buffer_size = (size_t)lua_tointeger(L, 2);
	size_t compress_threhold = (size_t)lua_tointeger(L, 3);
	if (max_fd <= 0 || buffer_size <= 0)
	{
		lua_pushnil(L);
		return 1;
	}

	auto mgr = new lua_socket_mgr();
	if (mgr->setup(G(L)->mainthread, max_fd, buffer_size, compress_threhold))
	{
		lua_push_object(L, mgr);
		return 1;
	}

	delete mgr;
	lua_pushnil(L);
	return 1;
}
