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

