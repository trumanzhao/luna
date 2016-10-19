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

static const char* g_code =
u8R"---(
luna_objects = luna_objects or {};

-- map<object_ptr, shadow_table>
setmetatable(luna_objects, {__mode = "v"});

function luna_export(ptr, meta)
    local tab = luna_objects[ptr];
    if not tab then
        tab = {};
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

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        puts(g_usage);
        return 1;
    }

	lua_State* L = luaL_newstate();

	luaL_openlibs(L);

	lua_register_function(L, "get_file_time", get_file_time);

    luaL_dostring(L, g_code);

    lua_call_global_function(L, "import", std::tie(), argv[1]);

	lua_close(L);
	return 0;
}


