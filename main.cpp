#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#include <signal.h>
#include "luna.h"
#include "tools.h"
#include "socket_io.h"

static const char* g_usage =
u8R"---(usage: luna main.lua)---";

static const char* g_code =
u8R"---(
luna_objects = luna_objects or {};

-- map<object_ptr, shadow_table>
setmetatable(luna_objects, {__mode = "v"});

function luna_export(ptr, meta)
    local tab = luna_objects[ptr];
    if not tab then
        tab = {__pointer__=ptr};
        setmetatable(tab, meta);
        luna_objects[ptr] = tab;
    end
    return tab;
end

luna_files = luna_files or {};
luna_file_meta = luna_file_meta or {__index=function(t, k) return _G[k]; end};

function import(filename)
    local file_module = luna_files[filename];
    if file_module then
        return file_module.env;
    end

    local env = {};
    setmetatable(env, luna_file_meta);

    local trunk = loadfile(filename, "bt", env);
    if not trunk then
        return nil;
    end

    local file_time = get_file_time(filename);
    luna_files[filename] = {filename=filename, time=file_time, env=env};

    local ok, err = pcall(trunk);
    if not ok then
        print(err);
        return nil;
    end

    return env;
end
)---";


struct fucker
{
    fucker(int id) : m_id(id) {}

    int add(int a, int b)
    {
        printf("add id=%d\n", m_id);
        return a + b;
    }

    void gc()
    {
        printf("gc id=%d\n", m_id);
    }

    int m_id;
    DECLARE_LUA_CLASS(fucker);
};

EXPORT_CLASS_BEGIN(fucker)
EXPORT_LUA_FUNCTION(add)
EXPORT_LUA_INT(m_id)
EXPORT_CLASS_END()

int get_fucker(lua_State* L)
{
    lua_push_object(L, new fucker(123));
    lua_push_object(L, new fucker(456));
    return 2;
}

fucker* same_fucker(fucker* o)
{
    printf("same %d\n", o->m_id);
    return o;
}

lua_State* g_lua = nullptr;

#if defined(__linux) || defined(__APPLE__)
static void on_quit_signal(int signo)
{
    lua_call_global_function(g_lua, "on_quit_signal", std::tie(), signo);
}
#endif

int main(int argc, const char* argv[])
{
#if defined(__linux) || defined(__APPLE__)
    tzset();
#endif

#ifdef _MSC_VER
	_tzset();
#endif

    setlocale(LC_ALL, "");

#if defined(__linux) || defined(__APPLE__)
    signal(SIGINT, on_quit_signal);
    signal(SIGTERM, on_quit_signal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#endif

	std::string err;
	auto* mgr = create_socket_mgr(1000);

	auto listener = mgr->listen(err, "127.0.0.1", 8080);
	if (!listener)
	{
		puts("failed listen");
		return 0;
	}

	mgr->set_listen_callback(listener, [](auto sm) {
		printf("new connection: %lld\n", sm);
	});

	auto connector = mgr->connect(err, "127.0.0.1", "8080", 1000);
	mgr->set_connect_callback(connector, [](auto sm) {
		printf("connect ok: %lld\n", sm);
	});

	mgr->set_error_callback(connector, [](auto err) {
		printf("connect err: %s\n", err);
	});
	
	while (true)
	{
		mgr->wait(100);
	}

    if (argc != 2)
    {
        puts(g_usage);
        return 1;
    }

	g_lua = luaL_newstate();

	luaL_openlibs(g_lua);
    luaL_dostring(g_lua, g_code);

    lua_register_function(g_lua, "get_file_time", get_file_time);
    lua_register_function(g_lua, "get_time_ms", get_time_ms);
    lua_register_function(g_lua, "sleep_ms", sleep_ms);

    lua_register_function(g_lua, "get_fucker", get_fucker);
    lua_register_function(g_lua, "same_fucker", same_fucker);

    lua_call_global_function(g_lua, "import", std::tie(), argv[1]);
    lua_call_global_function(g_lua, "main");

	lua_close(g_lua);
	return 0;
}


