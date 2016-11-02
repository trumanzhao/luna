/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

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
#include "socket_wapper.h"

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

lua_State* g_lvm = nullptr;

#if defined(__linux) || defined(__APPLE__)
static void on_quit_signal(int signo)
{
    lua_call_global_function(g_lvm, "on_quit_signal", std::tie(), signo);
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

    if (argc != 2)
    {
        puts(g_usage);
        return 1;
    }

	g_lvm = luaL_newstate();

	luaL_openlibs(g_lvm);
    luaL_dostring(g_lvm, g_code);

    lua_register_function(g_lvm, "get_file_time", get_file_time);
    lua_register_function(g_lvm, "get_time_ms", get_time_ms);
    lua_register_function(g_lvm, "sleep_ms", sleep_ms);
	lua_register_function(g_lvm, "create_socket_mgr", lua_create_socket_mgr);

    lua_call_global_function(g_lvm, "import", std::tie(), argv[1]);
    lua_call_global_function(g_lvm, "main");

	lua_close(g_lvm);
	return 0;
}


