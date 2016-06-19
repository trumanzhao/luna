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
#include "lua.hpp"

template <typename T> void lua_push_object(lua_State* L, T obj);
template <typename T> T lua_to_object(lua_State* L, int idx);

template <typename T> T lua_to_native(lua_State* L, int i) { return lua_to_object<T>(L, i); }
template <> inline  const char* lua_to_native<const char*>(lua_State* L, int i) { return lua_tostring(L, i); }
template <> inline int64_t lua_to_native<int64_t>(lua_State* L, int i) { return lua_tointeger(L, i); }
template <> inline int lua_to_native<int>(lua_State* L, int i) { return (int)lua_tointeger(L, i); }
template <> inline float lua_to_native<float>(lua_State* L, int i) { return (float)lua_tonumber(L, i); }
template <> inline double lua_to_native<double>(lua_State* L, int i) { return lua_tonumber(L, i); }
template <> inline std::string lua_to_native<std::string>(lua_State* L, int i)
{
	const char* str = lua_tostring(L, i);
	return str == nullptr ? "" : str;
}

template <typename T>
void native_to_lua(lua_State* L, T* v) { lua_push_object(L, v); }
inline void native_to_lua(lua_State* L, const char* v) { lua_pushstring(L, v); }
inline void native_to_lua(lua_State* L, int v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, int64_t v) { lua_pushinteger(L, v); }
inline void native_to_lua(lua_State* L, float v) { lua_pushnumber(L, v); }
inline void native_to_lua(lua_State* L, double v) { lua_pushnumber(L, v); }
inline void native_to_lua(lua_State* L, const std::string& v) { lua_pushstring(L, v.c_str()); }

typedef std::function<int(lua_State*)> lua_global_function;
typedef std::function<int(void*, lua_State*)> lua_object_function;

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

#define LUA_NATIVE_POINTER "__native_pointer__"

int Lua_object_bridge(lua_State* L);

template <typename T>
int lua_member_index(lua_State* L)
{
	T* obj = nullptr;
	const char* key = nullptr;
	const char* meta_name = nullptr;
	lua_member_item* item = nullptr;
	char* addr = nullptr;

	lua_pushstring(L, LUA_NATIVE_POINTER);
	lua_rawget(L, 1);

	obj = (T*)lua_touserdata(L, -1);
	if (obj == nullptr)
	{
		lua_pushnil(L);
		return 1;
	}

	lua_pop(L, 1);  // pop the userdata

	key = lua_tostring(L, 2);
	meta_name = obj->lua_get_meta_name();
	if (key == nullptr || meta_name == nullptr)
	{
		lua_pushnil(L);
		return 1;
	}

	luaL_getmetatable(L, meta_name);
	lua_pushstring(L, key);
	lua_rawget(L, -2);

	item = (lua_member_item*)lua_touserdata(L, -1);
	if (item == nullptr)
	{
		lua_pushnil(L);
		return 1;
	}

	lua_settop(L, 2);

	addr = (char*)obj + item->offset;

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
		lua_pushvalue(L, 1);
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
	T* obj = nullptr;
	const char* meta_name = nullptr;
	const char* key = nullptr;
	char* addr = nullptr;
	lua_member_item* item = nullptr;

	lua_pushstring(L, LUA_NATIVE_POINTER);
	lua_rawget(L, 1);

	obj = (T*)lua_touserdata(L, -1);
	if (obj == nullptr)
		return 0;

	lua_pop(L, 1);

	key = lua_tostring(L, 2);
	meta_name = obj->lua_get_meta_name();
	if (key == nullptr || meta_name == nullptr)
		return 0;

	luaL_getmetatable(L, meta_name);
	lua_pushvalue(L, 2);
	lua_rawget(L, -2);

	item = (lua_member_item*)lua_touserdata(L, -1);
	lua_pop(L, 2);
	if (item == nullptr)
	{
		lua_rawset(L, -3);
		return 0;
	}

	if (item->readonly)
		return 0;

	addr = (char*)obj + item->offset;

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
			if (str_len < item->size)
			{
				strcpy(addr, str);
			}
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

	while (item->type != lua_member_type::member_none)
	{
		lua_pushstring(L, item->name);
		lua_pushlightuserdata(L, item);
		lua_rawset(L, -3);
		item++;
	}

	lua_settop(L, top);
}

struct lua_obj_ref
{
	lua_obj_ref() {}
	lua_obj_ref(lua_obj_ref const&) = delete;
	lua_obj_ref& operator = (lua_obj_ref const&) { return *this; }

	~lua_obj_ref()
	{
		release();
	}

	void release()
	{
		if (m_lvm == nullptr || m_ref == LUA_NOREF)
			return;

		int top = lua_gettop(m_lvm);
		lua_rawgeti(m_lvm, LUA_REGISTRYINDEX, m_ref);
		if (lua_istable(m_lvm, -1))
		{
			lua_pushstring(m_lvm, LUA_NATIVE_POINTER);
			lua_pushnil(m_lvm);
			lua_rawset(m_lvm, -3);
			luaL_unref(m_lvm, LUA_REGISTRYINDEX, m_ref);
		}
		lua_settop(m_lvm, top);
		m_lvm = nullptr;
		m_ref = LUA_NOREF;
	}
	lua_State* m_lvm = nullptr;
	int m_ref = LUA_NOREF;
};

template <typename T>
void lua_push_object(lua_State* L, T obj)
{
	if (obj == nullptr)
	{
		lua_pushnil(L);
		return;
	}

	lua_obj_ref& ref = obj->lua_get_obj_ref();
	if (ref.m_ref != LUA_NOREF)
	{
		assert(ref.m_lvm == L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref.m_ref);
		return;
	}

	lua_newtable(L);
	lua_pushstring(L, LUA_NATIVE_POINTER);
	lua_pushlightuserdata(L, obj);
	lua_settable(L, -3);

	const char* meta_name = obj->lua_get_meta_name();
	luaL_getmetatable(L, meta_name);
	if (lua_isnil(L, -1))
	{
		lua_remove(L, -1);
		lua_register_class(L, obj);
		luaL_getmetatable(L, meta_name);
	}
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	ref.m_lvm = L;
	ref.m_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

template <typename T>
T lua_to_object(lua_State* L, int idx)
{
	T obj = nullptr;
	if (lua_istable(L, idx))
	{
		lua_getfield(L, idx, LUA_NATIVE_POINTER);
		obj = (T)lua_touserdata(L, -1);
		lua_pop(L, 1);
	}
	return obj;
}

template <typename T>
void lua_clear_ref(T* obj)
{
	lua_obj_ref& ref = obj->get_obj_ref();
	ref.release();
}

#define DECLARE_LUA_CLASS(ClassName)    \
    lua_obj_ref m_lua_obj_ref;  \
    lua_obj_ref& lua_get_obj_ref() { return m_lua_obj_ref; }    \
    const char* lua_get_meta_name() { return "_class_meta:"#ClassName; }    \
    lua_member_item* lua_get_meta_data();	\

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

void lua_setup_env(lua_State*);

// 注册全局函数
void lua_register_function(lua_State* L, const char* name, lua_global_function func);

template <typename T>
void lua_register_function(lua_State* L, const char* name, T func)
{
	lua_register_function(L, name, lua_adapter(func));
}

// 在一个指定环境表中执行脚本字符串
// code_len为-1时,会按照strlen(code)计算.
bool lua_load_string(lua_State* L, const char env[], const char code[], int code_len = -1);

// 加载文件,已加载的不会重复加载
bool lua_load_script(lua_State* L, const char file_name[]);

// 检查所有文件时间戳,重新加载已变更的文件
void lua_reload_update(lua_State* L);

// 从指定文件中获取函数,如果文件未加载则自动加载
bool lua_get_file_function(lua_State* L, const char file_name[], const char function[]);

// 从指定的全局table中获取函数
bool lua_get_table_function(lua_State* L, const char table[], const char function[]);

template <typename T>
bool lua_get_object_function(lua_State* L, T* object, const char function[])
{
	if (object == nullptr)
		return false;
	lua_push_object(L, object);
	lua_getfield(L, -1, function);
	if (!lua_isfunction(L, -1))
	{
		lua_pop(L, 2);
		return false;
	}
	lua_remove(L, -2);
	return true;
}

bool lua_call_function(lua_State* L, int arg_count, int ret_count);

// 设置错误处理函数以及文件读取函数,默认调用c库相关函数:
void lua_set_error_func(lua_State* L, std::function<void(const char*)>& error_func);
void lua_set_file_time_func(lua_State* L, std::function<bool(time_t*, const char*)>& time_func);
void lua_set_file_size_func(lua_State* L, std::function<bool(size_t*, const char*)>& size_func);
void lua_set_file_data_func(lua_State* L, std::function<bool(char*, size_t, const char*)>& data_func);

template<size_t... Integers, typename... var_types>
void lua_to_native_mutil(lua_State* L, std::tuple<var_types&...>& vars, std::index_sequence<Integers...>&&)
{
	constexpr int ret_count = sizeof...(Integers);
	int _[] = { 0, (std::get<Integers>(vars) = lua_to_native<var_types>(L, (int)Integers - ret_count), 0)... };
}

template <typename... ret_types, typename... arg_types>
bool lua_call_file_function(lua_State* L, const char file_name[], const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
	if (!lua_get_file_function(L, file_name, function))
		return false;

	int _0[] = { 0, (native_to_lua(L, args), 0)... };
	constexpr int ret_count = sizeof...(ret_types);
	if (!lua_call_function(L, sizeof...(arg_types), ret_count))
		return false;

	lua_to_native_mutil(L, rets, std::make_index_sequence<ret_count>());
	return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_table_function(lua_State* L, const char table[], const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
	if (!lua_get_table_function(L, table, function))
		return false;

	int _0[] = { 0, (native_to_lua(L, args), 0)... };
	constexpr int ret_count = sizeof...(ret_types);
	if (!lua_call_function(L, sizeof...(arg_types), ret_count))
		return false;

	lua_to_native_mutil(L, rets, std::make_index_sequence<ret_count>());
	return true;
}

template <typename T, typename... ret_types, typename... arg_types>
bool lua_call_object_function(lua_State* L, T* o, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
	if (!lua_get_object_function(L, o, function))
		return false;

	int _0[] = { 0, (native_to_lua(L, args), 0)... };
	constexpr int ret_count = sizeof...(ret_types);
	if (!lua_call_function(L, sizeof...(arg_types), ret_count))
		return false;

	lua_to_native_mutil(L, rets, std::make_index_sequence<ret_count>());
	return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_global_function(lua_State* L, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args)
{
	if (lua_getglobal(L, function) != LUA_OK || !lua_isfunction(L, -1))
		return false;

	int _0[] = { 0, (native_to_lua(L, args), 0)... };
	constexpr int ret_count = sizeof...(ret_types);
	if (!lua_call_function(L, sizeof...(arg_types), ret_count))
		return false;

	lua_to_native_mutil(L, rets, std::make_index_sequence<ret_count>());
	return true;
}

inline bool lua_call_file_function(lua_State* L, const char file_name[], const char function[]) { return lua_call_file_function(L, file_name, function, std::tie()); }
inline bool lua_call_table_function(lua_State* L, const char table[], const char function[]) { return lua_call_table_function(L, table, function, std::tie()); }
template <typename T> inline bool lua_call_object_function(lua_State* L, T* o, const char function[]) { return lua_call_object_function(L, o, function, std::tie()); }
inline bool lua_call_global_function(lua_State* L, const char function[]) { return lua_call_global_function(L, function, std::tie()); }

class lua_guard_t
{
	int m_top = 0;
	lua_State* m_lvm = nullptr;
public:
	lua_guard_t(lua_State* L) : m_lvm(L) { m_top = lua_gettop(L); }
	~lua_guard_t() { lua_settop(m_lvm, m_top); }
};
