#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#ifdef __linux
#include <dirent.h>
#endif
#include "luna.h"

static time_t get_file_time(const char* file_name, time_t n)
{
    if (file_name == nullptr)
        return 0;

    struct stat file_info;
    int ret = stat(file_name, &file_info);
    if (ret != 0)
        return 0;

#ifdef __APPLE__
    return file_info.st_mtimespec.tv_sec;
#endif

#if defined(_MSC_VER) || defined(__linux)
    return file_info.st_mtime;
#endif
}

static const char* g_usage =
u8R"---(usage: luna main.lua)---";

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        puts(g_usage);
        return 1;
    }

	lua_State* L = luaL_newstate();

	luaL_openlibs(L);

	lua_setup_env(L);

	lua_register_function(L, "get_file_time", get_file_time);

    luaL_dofile(L, argv[1]);

	lua_close(L);
	return 0;
}
