/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux
#include <dirent.h>
#endif
#include <map>
#include <string>
#include <algorithm>
#include <cstdio>
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

void lua_register_function(lua_State* L, const char* name, lua_global_function func)
{
    lua_push_object(L, new luna_function_wapper(func));
    lua_pushcclosure(L, Lua_global_bridge, 1);
    lua_setglobal(L, name);
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
        puts("call invalid function !");
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
        puts(lua_tostring(L, -1));
        return true;
    }
    lua_remove(L, -ret_count - 1); // remove 'traceback'
    return true;
}

static const char* luna_code =
u8R"---(
--map<native_ptr, shadow_table>
luna_objects = luna_objects or {};
setmetatable(luna_objects, { __mode = "v" });

function luna_export(ptr, meta)
    local tab = luna_objects[ptr];
    if not tab then
        tab = { __pointer__ = ptr };
        setmetatable(tab, meta);
        luna_objects[ptr] = tab;
    end
    return tab;
end

luna_files = luna_files or {};
luna_file_meta = luna_file_meta or {__index = function(t, k) return _G[k]; end};
luna_print = print;

function import(filename)
    local file_module = luna_files[filename];
    if file_module then
        return file_module.env;
    end

    local env = {};
    setmetatable(env, luna_file_meta);

    local trunk, msg = loadfile(filename, "bt", env);
    if not trunk then
		luna_print(string.format("load file: %s ... ... [failed]", filename));
		luna_print(msg);
        return nil;
    end

    local file_time = get_file_time(filename);
    luna_files[filename] = { filename = filename, time = file_time, env = env };

    local ok, err = pcall(trunk);
    if not ok then
		luna_print(string.format("load file: %s ... ... [failed]", filename));
        luna_print(err);
        return nil;
    end

	luna_print(string.format("load file: %s ... ... [ok]", filename));
    return env;
end

local try_reload_one = function(filename, env)
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

local try_reload_all = function()
    for filename, filenode in pairs(luna_files) do
        local filetime = get_file_time(filename);
        if filetime ~= 0 and filetime ~= filenode.time then
            filenode.time = filetime;
			local ok, err = try_reload_one(filename, filenode.env);
			luna_print(string.format("load file: %s ... ... [%s]", filename, ok and "ok" or "failed"));
			if not ok then
				luna_print(err);
			end
        end
    end
end

luna_quit_flag = false;
function luna_entry(filename)
    local entry_file = import(filename);
    if entry_file == nil then
        return;
    end

    local next_reload_time = 0;
    while not luna_quit_flag do
        local now = get_time_ms();
        local on_loop = entry_file.on_loop;

        if on_loop then
            pcall(on_loop, now);
        end

        if now >= next_reload_time then
            try_reload_all();
            next_reload_time = now + 3000;
        end
    end
end
)---";

#ifdef _MSC_VER
void daemon() {  } // do nothing !
#endif

bool luna_setup(lua_State* L)
{
    luaL_openlibs(L);

	int ret = luaL_dostring(L, luna_code);
	if (ret != 0)
		return false;

    lua_register_function(L, "get_file_time", get_file_time);
    lua_register_function(L, "get_time_ns", get_time_ns);
    lua_register_function(L, "get_time_ms", get_time_ms);
    lua_register_function(L, "sleep_ms", sleep_ms);
    lua_register_function(L, "daemon", daemon);
    lua_register_function(L, "create_socket_mgr", lua_create_socket_mgr);

    return  true;
}

