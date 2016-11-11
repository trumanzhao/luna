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

lua_State* g_lvm = nullptr;

static void on_quit_signal(int signo)
{
    lua_call_global_function(g_lvm, "on_quit_signal", std::tie(), signo);
}

#ifdef _MSC_VER
void daemon() {  } // do nothing !
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

    signal(SIGINT, on_quit_signal);
    signal(SIGTERM, on_quit_signal);

#if defined(__linux) || defined(__APPLE__)
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#endif

    g_lvm = luaL_newstate();

    luaL_openlibs(g_lvm);
    int ret = luaL_dofile(g_lvm, "base/base.lua");
    if (ret == 0)
    {
        lua_register_function(g_lvm, "get_file_time", get_file_time);
        lua_register_function(g_lvm, "get_time_ns", get_time_ns);
        lua_register_function(g_lvm, "get_time_ms", get_time_ms);
        lua_register_function(g_lvm, "sleep_ms", sleep_ms);
		lua_register_function(g_lvm, "daemon", daemon);
        lua_register_function(g_lvm, "create_socket_mgr", lua_create_socket_mgr);

        lua_guard_t g(g_lvm);
        lua_call_global_function(g_lvm, "luna_entry", std::tie(), argc > 1 ? argv[1] : "main.lua");
    }

    lua_close(g_lvm);
    return 0;
}


