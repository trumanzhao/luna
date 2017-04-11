/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux
#include <dirent.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <map>
#include <string>
#include <algorithm>
#include "luna.h"
#include "tools.h"
#include "socket_io.h"
#include "socket_wapper.h"

struct luna_function_wapper final
{
    luna_function_wapper(const lua_global_function& func) : m_func(func) {}
    lua_global_function m_func;
    DECLARE_LUA_CLASS(luna_function_wapper);
};

EXPORT_CLASS_BEGIN(luna_function_wapper)
EXPORT_CLASS_END()

static int Lua_global_bridge(lua_State* L)
{
    auto* wapper  = lua_to_object<luna_function_wapper*>(L, lua_upvalueindex(1));
    if (wapper != nullptr)
    {
        return wapper->m_func(L);
    }
    return 0;
}

void lua_push_function(lua_State* L, lua_global_function func)
{
    lua_push_object(L, new luna_function_wapper(func));
    lua_pushcclosure(L, Lua_global_bridge, 1);
}

int Lua_object_bridge(lua_State* L)
{
    void* obj = lua_touserdata(L, lua_upvalueindex(1));
    lua_object_function* func = (lua_object_function*)lua_touserdata(L, lua_upvalueindex(2));
    if (obj != nullptr && func != nullptr)
    {
        return (*func)(obj, L);
    }
    return 0;
}

bool lua_get_table_function(lua_State* L, const char table[], const char function[])
{
    lua_getglobal(L, table);
    lua_getfield(L, -1, function);
    lua_remove(L, -2);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool lua_call_function(lua_State* L, int arg_count, int ret_count)
{
    int func_idx = lua_gettop(L) - arg_count;
    if (func_idx <= 0 || !lua_isfunction(L, func_idx))
    {
		perror("call invalid function !");
        return false;
    }

    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_remove(L, -2); // remove 'debug'

    lua_insert(L, func_idx);
    if (lua_pcall(L, arg_count, ret_count, func_idx))
    {
        // 注意,该函数只在指定的函数不存在时才返回false
        // lua函数内部执行出错时,并不认为是错误,这是因为lua函数执行时,可能时中途错误,而前面已经执行了部分逻辑
        // 如果这种情况返回false,上层逻辑会不好处理,比如: 我push的一个对象到底被lua引用了没?我要删除它么?
        perror(lua_tostring(L, -1));
        return true;
    }
    lua_remove(L, -ret_count - 1); // remove 'traceback'
    return true;
}

static const char* luna_code =
u8R"---(
luna = luna or {files={}, meta={__index = function(t, k) return _G[k]; end}, print=print, objects={}};
setmetatable(luna.objects, { __mode = "v" });

function export(ptr, meta)
    local tab = luna.objects[ptr];
    if not tab then
        tab = { __pointer__ = ptr };
        setmetatable(tab, meta);
        luna.objects[ptr] = tab;
    end
    return tab;
end

function import(filename)
    local file_module = luna.files[filename];
    if file_module then
        return file_module.env;
    end

	local env = {};
    setmetatable(env, luna.meta);

	local trunk, msg = loadfile(filename, "bt", env);
    if not trunk then
        luna.print(string.format("load file: %s ... ... [failed]", filename));
        luna.print(msg);
        return nil;
    end

	local file_time = luna.get_file_time(filename);
    luna.files[filename] = {filename=filename, time=file_time, env=env };

	local ok, err = pcall(trunk);
    if not ok then
        luna.print(string.format("load file: %s ... ... [failed]", filename));
        luna.print(err);
        return nil;
    end

	luna.print(string.format("load file: %s ... ... [ok]", filename));
    return env;
end

local reload_script = function(filename, env)
    local trunk, msg = loadfile(filename, "bt", env);
    if not trunk then
        return false, msg;
    end

	local ok, err = pcall(trunk);
    if not ok then
        return false, err;
    end
    return true, nil;
end

luna.try_reload = function()
    for filename, filenode in pairs(luna.files) do
        local filetime = luna.get_file_time(filename);
        if filetime ~= 0 and filetime ~= filenode.time then
            filenode.time = filetime;
            local ok, err = reload_script(filename, filenode.env);
            luna.print(string.format("load file: %s ... ... [%s]", filename, ok and "ok" or "failed"));
            if not ok then
                luna.print(err);
            end
        end
    end
end

luna.entry = function(filename)
    local entry_file = import(filename);
    if entry_file and entry_file.main then
        entry_file.main();
    end
end
)---";

#ifdef _MSC_VER
void daemon() {  } // do nothing !
#endif

static bool g_quit_signal = false;
static void on_quit_signal(int signo) { g_quit_signal = true; }
static bool get_guit_signal() { return g_quit_signal; }

#define luna_new_function(L, func) lua_push_function(L, func), lua_setfield(L, -2, #func)

bool luna_setup(lua_State* L)
{
	lua_guard g(L);
    luaL_openlibs(L);

    int ret = luaL_dostring(L, luna_code);
	if (ret != 0)
	{
		perror(lua_tostring(L, -1));
		return false;
	}

	signal(SIGINT, on_quit_signal);
	signal(SIGTERM, on_quit_signal);

	lua_getglobal(L, "luna");
	luna_new_function(L, get_file_time);
	luna_new_function(L, get_time_ms);
	luna_new_function(L, get_time_ns);
	luna_new_function(L, sleep_ms);
	luna_new_function(L, daemon);
	luna_new_function(L, create_socket_mgr);
	luna_new_function(L, get_guit_signal);
	lua_pop(L, 1);
	
    return  true;
}

