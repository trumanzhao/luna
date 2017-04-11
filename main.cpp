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

#ifdef _MSC_VER
#define tzset _tzset
#endif

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        puts("luna entry.lua [options ...]");
        return 1;
    }

    tzset();
    setlocale(LC_ALL, "");

#if defined(__linux) || defined(__APPLE__)
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#endif

    g_lvm = luaL_newstate();
    if (luna_setup(g_lvm))
    {
        lua_guard g(g_lvm);
        lua_call_table_function(g_lvm, "luna", "entry", std::tie(), argv[1]);
    }

    lua_close(g_lvm);
    return 0;
}
