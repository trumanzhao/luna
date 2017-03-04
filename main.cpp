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

lua_State* g_lvm = nullptr;

static bool g_quit_signal = false;
static void on_quit_signal(int signo) { g_quit_signal = true; }
static bool get_guit_signal() { return g_quit_signal; }

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
    if (luna_setup(g_lvm))
    {
		lua_register_function(g_lvm, "get_guit_signal", get_guit_signal);

        lua_guard g(g_lvm);
        lua_call_global_function(g_lvm, "luna_entry", std::tie(), argc > 1 ? argv[1] : "main.lua");
    }

    lua_close(g_lvm);
    return 0;
}
