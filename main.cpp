/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <locale>
#include <stdint.h>
#include <signal.h>
#include "luna.h"
#include "tools.h"

lua_State* g_lvm = nullptr;

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
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
#endif

    g_lvm = luaL_newstate();
    if (luna_setup(g_lvm))
    {
        lua_guard g(g_lvm);
		lua_call_table_function(g_lvm, "luna", "entry", std::tie(), argc > 1 ? argv[1] : "main.lua");
    }

    lua_close(g_lvm);
    return 0;
}
