/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/06/18, trumanzhao@foxmail.com
*/

#pragma once

#include <assert.h>
#include <string.h>
#include <cstdint>
#include <string>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include "lua/lua.hpp"

template <typename T> void lua_push_object(lua_State* L, T obj);
template <typename T> T lua_to_object(lua_State* L, int idx);

template <typename T>
T lua_to_native(lua_State* L, int i) { return lua_to_object<T>(L, i); }

template <> inline  bool lua_to_native<bool>(lua_State* L, int i) { return lua_toboolean(L, i) != 0; }
template <> inline  char lua_to_native<char>(lua_State* L, int i) { return (char)lua_tointeger(L, i); }
template <> inline  unsigned char lua_to_native<unsigned char>(lua_State* L, int i) { return (unsigned char)lua_tointeger(L, i); }
template <> inline short lua_to_native<short>(lua_State* L, int i) { return (short)lua_tointeger(L, i); }
template <> inline unsigned short lua_to_native<unsigned short>(lua_State* L, int i) { return (unsigned short)lua_tointeger(L, i); }
template <> inline int lua_to_native<int>(lua_State* L, int i) { return (int)lua_tointeger(L, i); }
template <> inline unsigned int lua_to_native<unsigned int>(lua_State* L, int i) { return (unsigned int)lua_tointeger(L, i); }
template <> inline long lua_to_native<long>(lua_State* L, int i) { return (long)lua_tointeger(L, i); }
template <> inline unsigned long lua_to_native<unsigned long>(lua_State* L, int i) { return (unsigned long)lua_tointeger(L, i); }
template <> inline long long lua_to_native<long long>(lua_State* L, int i) { return lua_tointeger(L, i); }
template <> inline unsigned long long lua_to_native<unsigned long long>(lua_State* L, int i) { return (unsigned long long)lua_tointeger(L, i); }
template <> inline float lua_to_native<float>(lua_State* L, int i) { return (float)lua_tonumber(L, i); }
template <> inline double lua_to_native<double>(lua_State* L, int i) { return lua_tonumber(L, i); }
template <> inline  const char* lua_to_native<const char*>(lua_State* L, int i) { return lua_tostring(L, i); }
template <> inline std::string lua_to_native<std::string>(lua_State* L, int i)
{
    const char* str = lua_tostring(L, i);
    return str == nullptr ? "" : str;
}

template <typename T>
void native_to_lua(lua_State* L, T* v) { lua_push_object(L, v); }
inline void native_to_lua(lua_State* L, bool v) { lua_pushboolean(L, v); }
inline void native_to_lua(lua_State* L, char v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, unsigned char v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, short v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, unsigned short v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, int v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, unsigned int v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, long v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, unsigned long v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, long long v) { lua_pushinteger(L, (lua_Integer)v); }
inline void native_to_lua(lua_State* L, unsigned long long v) { lua_pushinteger(L, (lua_Integer)v); }
inline void native_to_lua(lua_State* L, float v) { lua_pushnumber(L, v); }
inline void native_to_lua(lua_State* L, double v) { lua_pushnumber(L, v); }
inline void native_to_lua(lua_State* L, const char* v) { lua_pushstring(L, v); }
inline void native_to_lua(lua_State* L, const std::string& v) { lua_pushstring(L, v.c_str()); }

using lua_global_function = std::function<int(lua_State*)>;
using lua_object_function = std::function<int(void*, lua_State*)>;

template<size_t... Integers, typename return_type, typename... arg_types>
return_type call_helper(lua_State* L, return_type(*func)(arg_types...), std::index_sequence<Integers...>&&)
{
    return (*func)(lua_to_native<arg_types>(L, Integers + 1)...);
}

template<size_t... Integers, typename return_type, typename class_type, typename... arg_types>
return_type call_helper(lua_State* L, class_type* obj, return_type(class_type::*func)(arg_types...), std::index_sequence<Integers...>&&)
{
    return (obj->*func)(lua_to_native<arg_types>(L, Integers + 1)...);
}

template <typename return_type, typename... arg_types>
lua_global_function lua_adapter(return_type(*func)(arg_types...))
{
    return [=](lua_State* L)
    {
        native_to_lua(L, call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename... arg_types>
lua_global_function lua_adapter(void(*func)(arg_types...))
{
    return [=](lua_State* L)
    {
        call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <>
inline lua_global_function lua_adapter(int(*func)(lua_State* L))
{
    return func;
}

template <typename return_type, typename T, typename... arg_types>
lua_object_function lua_adapter(return_type(T::*func)(arg_types...))
{
    return [=](void* obj, lua_State* L)
    {
        native_to_lua(L, call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename T, typename... arg_types>
lua_object_function lua_adapter(void(T::*func)(arg_types...))
{
    return [=](void* obj, lua_State* L)
    {
        call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <typename T>
lua_object_function lua_adapter(int(T::*func)(lua_State* L))
{
    return [=](void* obj, lua_State* L)
    {
        T* this_ptr = (T*)obj;
        return (this_ptr->*func)(L);
    };
}

enum class lua_member_type
{
    member_none,
    member_char,
    member_short,
    member_int,
    member_int64,
    member_time,
    member_bool,
    member_float,
    member_double,
    member_string,
    member_std_str,
    member_function
};

struct lua_member_item
{
    const char* name;
    lua_member_type type;
    int offset;
    size_t size;
    bool readonly;
    lua_object_function func;
};

int Lua_object_bridge(lua_State* L);

template <typename T>
int lua_member_index(lua_State* L)
{
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    const char* key = lua_tostring(L, 2);
    const char* meta_name = obj->lua_get_meta_name();
    if (key == nullptr || meta_name == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    luaL_getmetatable(L, meta_name);
    lua_pushstring(L, key);
    lua_rawget(L, -2);

    auto item = (lua_member_item*)lua_touserdata(L, -1);
    if (item == nullptr)
    {
        lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 2);

    char* addr = (char*)obj + item->offset;

    switch (item->type)
    {
    case lua_member_type::member_char:
        assert(item->size == sizeof(char));
        lua_pushinteger(L, *(char*)addr);
        break;

    case lua_member_type::member_short:
        assert(item->size == sizeof(short));
        lua_pushinteger(L, *(short*)addr);
        break;

    case lua_member_type::member_int:
        assert(item->size == sizeof(int));
        lua_pushinteger(L, *(int*)addr);
        break;

    case lua_member_type::member_int64:
        assert(item->size == sizeof(int64_t));
        lua_pushinteger(L, *(int64_t*)addr);
        break;

    case lua_member_type::member_time:
        assert(item->size == sizeof(time_t));
        lua_pushinteger(L, *(time_t*)addr);
        break;

    case lua_member_type::member_bool:
        assert(item->size == sizeof(bool));
        lua_pushboolean(L, *(bool*)addr);
        break;

    case lua_member_type::member_float:
        assert(item->size == sizeof(float));
        lua_pushnumber(L, *(float*)addr);
        break;

    case lua_member_type::member_double:
        assert(item->size == sizeof(double));
        lua_pushnumber(L, *(double*)addr);
        break;

    case lua_member_type::member_string:
        lua_pushstring(L, (const char*)addr);
        break;

    case lua_member_type::member_std_str:
        lua_pushstring(L, ((std::string*)addr)->c_str());
        break;

    case lua_member_type::member_function:
        lua_pushlightuserdata(L, obj);
        lua_pushlightuserdata(L, &item->func);
        lua_pushcclosure(L, Lua_object_bridge, 2);
        break;

    default:
        lua_pushnil(L);
    }

    return 1;
}

template <typename T>
int lua_member_new_index(lua_State* L)
{
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
        return 0;

    const char* key = lua_tostring(L, 2);
    const char* meta_name = obj->lua_get_meta_name();
    if (key == nullptr || meta_name == nullptr)
        return 0;

    luaL_getmetatable(L, meta_name);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    auto item = (lua_member_item*)lua_touserdata(L, -1);
    lua_pop(L, 2);
    if (item == nullptr)
    {
        lua_rawset(L, -3);
        return 0;
    }

    if (item->readonly)
        return 0;

    char* addr = (char*)obj + item->offset;

    switch (item->type)
    {
    case lua_member_type::member_char:
        assert(item->size == sizeof(char));
        *addr = (char)lua_tointeger(L, -1);
        break;

    case lua_member_type::member_short:
        assert(item->size == sizeof(short));
        *(short*)addr = (short)lua_tointeger(L, -1);
        break;

    case lua_member_type::member_int:
        assert(item->size == sizeof(int));
        *(int*)addr = (int)lua_tointeger(L, -1);
        break;

    case lua_member_type::member_int64:
        assert(item->size == sizeof(int64_t));
        *(int64_t*)addr = (int64_t)lua_tointeger(L, -1);
        break;

    case lua_member_type::member_time:
        assert(item->size == sizeof(time_t));
        *(time_t*)addr = (time_t)lua_tointeger(L, -1);
        break;

    case lua_member_type::member_bool:
        assert(item->size == sizeof(bool));
        *(bool*)addr = !!lua_toboolean(L, -1);
        break;

    case lua_member_type::member_float:
        assert(item->size == sizeof(float));
        *(float*)addr = (float)lua_tonumber(L, -1);
        break;

    case lua_member_type::member_double:
        assert(item->size == sizeof(double));
        *(double*)addr = (double)lua_tonumber(L, -1);
        break;

    case lua_member_type::member_string:
        if (lua_isstring(L, -1))
        {
            const char* str = lua_tostring(L, -1);
            size_t str_len = strlen(str);
            if (str_len >= item->size)
            {
                str_len = item->size - 1;
            }
            memcpy(addr, str, str_len + 1);
        }
        break;

    case lua_member_type::member_std_str:
        if (lua_isstring(L, -1))
        {
            *(std::string*)addr = lua_tostring(L, -1);
        }
        break;

    case lua_member_type::member_function:
        lua_rawset(L, -3);
        break;

    default:
        break;
    }

    return 0;
}

template<typename T>
struct has_member_gc
{
    template<typename U> static auto check(int) -> decltype(std::declval<U>().__gc(), std::true_type());
    template<typename U> static std::false_type check(...);
    enum { value = std::is_same<decltype(check<T>(0)), std::true_type>::value };
};

template <class T>
typename std::enable_if<has_member_gc<T>::value, void>::type lua_handle_gc(T* obj) { obj->__gc(); }

template <class T>
typename std::enable_if<!has_member_gc<T>::value, void>::type lua_handle_gc(T* obj) { delete obj; }

template <typename T>
int lua_object_gc(lua_State* L)
{
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
    {
        perror("__gc error: nullptr !");
        return 0;
    }
    lua_handle_gc(obj);
    return 0;
}

template <typename T>
void lua_register_class(lua_State* L, T* obj)
{
    int top = lua_gettop(L);
    const char* meta_name = obj->lua_get_meta_name();
    lua_member_item* item = obj->lua_get_meta_data();

    luaL_newmetatable(L, meta_name);
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, &lua_member_index<T>);
    lua_rawset(L, -3);

    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, &lua_member_new_index<T>);
    lua_rawset(L, -3);

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, &lua_object_gc<T>);
    lua_rawset(L, -3);

    while (item->type != lua_member_type::member_none)
    {
        const char* name = item->name;
        // export member name "m_xxx" as "xxx"
#if !defined(LUNA_KEEP_MEMBER_PREFIX)
        if (name[0] == 'm' && name[1] == '_')
            name += 2;
#endif
        lua_pushstring(L, name);
        lua_pushlightuserdata(L, item);
        lua_rawset(L, -3);
        item++;
    }

    lua_settop(L, top);
}

template <typename T>
void lua_push_object(lua_State* L, T obj)
{
    // 禁止将对象的父类指针push到lua中去,以免造成指针转换的低级错误(在lua_push_object时).
    // 如果自信不会犯这种错误,并且懒得写final,可以将下一行注释掉:)
    static_assert(std::is_final<typename std::remove_pointer<T>::type>::value, "T should be declared final !");
    if (obj == nullptr)
    {
        lua_pushnil(L);
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "__objects__");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_newtable(L);
        lua_pushstring(L, "v");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);

        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "__objects__");
    }

    // stack: __objects__
    if (lua_rawgetp(L, -1, obj) != LUA_TTABLE)
    {
        lua_pop(L, 1);

        lua_newtable(L);
        lua_pushstring(L, "__pointer__");
        lua_pushlightuserdata(L, obj);
        lua_rawset(L, -3);

        // stack: __objects__, tab
        const char* meta_name = obj->lua_get_meta_name();
        luaL_getmetatable(L, meta_name);
        if (lua_isnil(L, -1))
        {
            lua_remove(L, -1);
            lua_register_class(L, obj);
            luaL_getmetatable(L, meta_name);
        }
        lua_setmetatable(L, -2);

        // stack: __objects__, tab
        lua_pushvalue(L, -1);
        lua_rawsetp(L, -3, obj);
    }
    lua_remove(L, -2);
}

template<typename T>
struct has_meta_data
{
    template<typename U> static auto check(int) -> decltype(std::declval<U>().lua_get_meta_data(), std::true_type());
    template<typename U> static std::false_type check(...);
    enum { value = std::is_same<decltype(check<T>(0)), std::true_type>::value };
};

template <typename T>
T lua_to_object(lua_State* L, int idx)
{
    static_assert(has_meta_data<typename std::remove_pointer<T>::type>::value, "T should be declared export !");
    static_assert(std::is_final<typename std::remove_pointer<T>::type>::value, "T should be declared final !");
    T obj = nullptr;
     if (lua_istable(L, idx))
     {
         lua_getfield(L, idx, "__pointer__");
         obj = (T)lua_touserdata(L, -1);
         lua_pop(L, 1);
     }
     return obj;
}

#define DECLARE_LUA_CLASS(ClassName)    \
    const char* lua_get_meta_name() { return "_class_meta:"#ClassName; }    \
    lua_member_item* lua_get_meta_data();   \

#define EXPORT_CLASS_BEGIN(ClassName)   \
lua_member_item* ClassName::lua_get_meta_data()   \
{   \
    typedef ClassName   class_type;  \
    static lua_member_item s_member_list[] = \
    {

#define EXPORT_CLASS_END()    \
        { nullptr, lua_member_type::member_none , 0, 0, false,  nullptr}  \
    };  \
    return s_member_list;  \
}

#define EXPORT_LUA_MEMBER(Type, Member, Name, ReadOnly)  {Name, Type, offsetof(class_type, Member), sizeof(((class_type*)nullptr)->Member), ReadOnly, nullptr},

#define EXPORT_LUA_CHAR_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_char, Member, Name, false)
#define EXPORT_LUA_CHAR_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_char, Member, Name, true)
#define EXPORT_LUA_CHAR(Member)   EXPORT_LUA_CHAR_AS(Member, #Member)
#define EXPORT_LUA_CHAR_R(Member)   EXPORT_LUA_CHAR_AS_R(Member, #Member)

#define EXPORT_LUA_SHORT_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_short, Member, Name, false)
#define EXPORT_LUA_SHORT_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_short, Member, Name, true)
#define EXPORT_LUA_SHORT(Member)   EXPORT_LUA_SHORT_AS(Member, #Member)
#define EXPORT_LUA_SHORT_R(Member)   EXPORT_LUA_SHORT_AS_R(Member, #Member)

#define EXPORT_LUA_INT_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_int, Member, Name, false)
#define EXPORT_LUA_INT_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_int, Member, Name, true)
#define EXPORT_LUA_INT(Member)   EXPORT_LUA_INT_AS(Member, #Member)
#define EXPORT_LUA_INT_R(Member)   EXPORT_LUA_INT_AS_R(Member, #Member)

#define EXPORT_LUA_INT64_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_int64, Member, Name, false)
#define EXPORT_LUA_INT64_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_int64, Member, Name, true)
#define EXPORT_LUA_INT64(Member)   EXPORT_LUA_INT64_AS(Member, #Member)
#define EXPORT_LUA_INT64_R(Member)   EXPORT_LUA_INT64_AS_R(Member, #Member)

#define EXPORT_LUA_TIME_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_time, Member, Name, false)
#define EXPORT_LUA_TIME_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_time, Member, Name, true)
#define EXPORT_LUA_TIME(Member)   EXPORT_LUA_TIME_AS(Member, #Member)
#define EXPORT_LUA_TIME_R(Member)   EXPORT_LUA_TIME_AS_R(Member, #Member)

#define EXPORT_LUA_BOOL_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_bool, Member, Name, false)
#define EXPORT_LUA_BOOL_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_bool, Member, Name, true)
#define EXPORT_LUA_BOOL(Member)   EXPORT_LUA_BOOL_AS(Member, #Member)
#define EXPORT_LUA_BOOL_R(Member)   EXPORT_LUA_BOOL_AS_R(Member, #Member)

#define EXPORT_LUA_FLOAT_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_float, Member, Name, false)
#define EXPORT_LUA_FLOAT_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_float, Member, Name, true)
#define EXPORT_LUA_FLOAT(Member)   EXPORT_LUA_FLOAT_AS(Member, #Member)
#define EXPORT_LUA_FLOAT_R(Member)   EXPORT_LUA_FLOAT_AS_R(Member, #Member)

#define EXPORT_LUA_DOUBLE_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_double, Member, Name, false)
#define EXPORT_LUA_DOUBLE_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_double, Member, Name, true)
#define EXPORT_LUA_DOUBLE(Member)   EXPORT_LUA_DOUBLE_AS(Member, #Member)
#define EXPORT_LUA_DOUBLE_R(Member)   EXPORT_LUA_DOUBLE_AS_R(Member, #Member)

#define EXPORT_LUA_STRING_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_string, Member, Name, false)
#define EXPORT_LUA_STRING_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_string, Member, Name, true)
#define EXPORT_LUA_STRING(Member)   EXPORT_LUA_STRING_AS(Member, #Member)
#define EXPORT_LUA_STRING_R(Member)   EXPORT_LUA_STRING_AS_R(Member, #Member)

#define EXPORT_LUA_STD_STR_AS(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_std_str, Member, Name, false)
#define EXPORT_LUA_STD_STR_AS_R(Member, Name)   EXPORT_LUA_MEMBER(lua_member_type::member_std_str, Member, Name, true)
#define EXPORT_LUA_STD_STR(Member)   EXPORT_LUA_STD_STR_AS(Member, #Member)
#define EXPORT_LUA_STD_STR_R(Member)   EXPORT_LUA_STD_STR_AS_R(Member, #Member)

#define EXPORT_LUA_FUNCTION_AS(Member, Name) { Name, lua_member_type::member_function, 0, 0, false, lua_adapter(&class_type::Member) },
#define EXPORT_LUA_FUNCTION_AS_R(Member, Name) { Name, lua_member_type::member_function, 0, 0, true, lua_adapter(&class_type::Member) },
#define EXPORT_LUA_FUNCTION(Member) EXPORT_LUA_FUNCTION_AS(Member, #Member)
#define EXPORT_LUA_FUNCTION_R(Member) EXPORT_LUA_FUNCTION_AS_R(Member, #Member)

void lua_push_function(lua_State* L, lua_global_function func);
inline void lua_push_function(lua_State* L, lua_CFunction func) { lua_pushcfunction(L, func); }

template <typename T>
void lua_push_function(lua_State* L, T func)
{
    lua_push_function(L, lua_adapter(func));
}

template <typename T>
void lua_register_function(lua_State* L, const char* name, T func)
{
    lua_push_function(L, func);
    lua_setglobal(L, name);
}


inline bool lua_get_global_function(lua_State* L, const char function[])
{
    lua_getglobal(L, function);
    return lua_isfunction(L, -1);
}

bool lua_get_table_function(lua_State* L, const char table[], const char function[]);

template <typename T>
bool lua_get_object_function(lua_State* L, T* object, const char function[])
{
    lua_push_object(L, object);
	if (!lua_istable(L, -1))
		return false;
    lua_getfield(L, -1, function);
    lua_remove(L, -2);
    return lua_isfunction(L, -1);
}

template<size_t... Integers, typename... var_types>
void lua_to_native_mutil(lua_State* L, std::tuple<var_types&...>& vars, std::index_sequence<Integers...>&&)
{
    int _[] = { 0, (std::get<Integers>(vars) = lua_to_native<var_types>(L, (int)Integers - sizeof...(Integers)), 0)... };
}

bool lua_call_function(std::string& err, lua_State* L, int arg_count, int ret_count);

template <typename... ret_types, typename... arg_types>
bool lua_call_function(std::string& err, lua_State* L, std::tuple<ret_types&...>&& rets, arg_types... args)
{
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(err, L, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_table_function(std::string& err, lua_State* L, const char table[], const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
    lua_get_table_function(L, table, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(err, L, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

template <typename T, typename... ret_types, typename... arg_types>
bool lua_call_object_function(std::string& err, lua_State* L, T* o, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
    lua_get_object_function(L, o, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(err, L, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_global_function(std::string& err, lua_State* L, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
    lua_getglobal(L, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(err, L, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    return true;
}

inline bool lua_call_table_function(std::string& err, lua_State* L, const char table[], const char function[]) { return lua_call_table_function(err, L, table, function, std::tie()); }
template <typename T> inline bool lua_call_object_function(std::string& err, lua_State* L, T* o, const char function[]) { return lua_call_object_function(err, L, o, function, std::tie()); }
inline bool lua_call_global_function(std::string& err, lua_State* L, const char function[]) { return lua_call_global_function(err, L, function, std::tie()); }

class lua_guard
{
public:
    lua_guard(lua_State* L) : m_lvm(L) { m_top = lua_gettop(L); }
    ~lua_guard() { lua_settop(m_lvm, m_top); }
    lua_guard(const lua_guard& other) = delete;
    lua_guard(lua_guard&& other) = delete;
    lua_guard& operator =(const lua_guard&) = delete;
private:
    int m_top = 0;
    lua_State* m_lvm = nullptr;
};


