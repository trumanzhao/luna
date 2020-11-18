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

template <typename T>
T lua_to_native(lua_State* L, int i) {
    if constexpr (std::is_same_v<T, bool>) {
        return lua_toboolean(L, i) != 0;
    } else if constexpr (std::is_same_v<T, std::string>) {
        const char* str = lua_tostring(L, i);
        return str == nullptr ? "" : str;
    } else if constexpr (std::is_integral_v<T>) {
        return (T)lua_tointeger(L, i);
    } else if constexpr (std::is_floating_point_v<T>) {
        return (T)lua_tonumber(L, i);
    } else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_volatile_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_same_v<type, const char>) {
            return lua_tostring(L, i);
        } else {
            return lua_to_object<T>(L, i); 
        }
    } else {
        // unsupported type
    }
}

template <typename T>
void native_to_lua(lua_State* L, T v) {
    if constexpr (std::is_same_v<T, bool>) {
        lua_pushboolean(L, v);
    } else if constexpr (std::is_same_v<T, std::string>) {
        lua_pushstring(L, v.c_str());
    } else if constexpr (std::is_integral_v<T>) {
        lua_pushinteger(L, (lua_Integer)v);
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, v);
    } else if constexpr (std::is_pointer_v<T>) {
        using type = std::remove_cv_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_same_v<type, char>) {
            if (v != nullptr) {
                lua_pushstring(L, v);
            } else {
                lua_pushnil(L);
            }
        } else {
            lua_push_object(L, v); 
        }
    } else {
        // unsupported type
        lua_pushnil(L);
    }
}

inline int lua_normal_index(lua_State* L, int idx) {
    int top = lua_gettop(L);
    if (idx < 0 && -idx <= top)
        return idx + top + 1;
    return idx;
}

bool _lua_set_fence(lua_State* L, const char fence[]);
void _lua_del_fence(lua_State* L, const char fence[]);

using lua_global_function = std::function<int(lua_State*)>;
using lua_object_function = std::function<int(void*, lua_State*)>;

template<size_t... integers, typename return_type, typename... arg_types>
return_type call_helper(lua_State* L, return_type(*func)(arg_types...), std::index_sequence<integers...>&&) {
    return (*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template<size_t... integers, typename return_type, typename class_type, typename... arg_types>
return_type call_helper(lua_State* L, class_type* obj, return_type(class_type::*func)(arg_types...), std::index_sequence<integers...>&&) {
    return (obj->*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template<size_t... integers, typename return_type, typename class_type, typename... arg_types>
return_type call_helper(lua_State* L, class_type* obj, return_type(class_type::*func)(arg_types...) const, std::index_sequence<integers...>&&) {
    return (obj->*func)(lua_to_native<arg_types>(L, integers + 1)...);
}

template <typename return_type, typename... arg_types>
lua_global_function lua_adapter(return_type(*func)(arg_types...)) {
    return [=](lua_State* L) {
        native_to_lua(L, call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename... arg_types>
lua_global_function lua_adapter(void(*func)(arg_types...)) {
    return [=](lua_State* L) {
        call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <>
inline lua_global_function lua_adapter(int(*func)(lua_State* L)) {
    return func;
}

template <typename return_type, typename T, typename... arg_types>
lua_object_function lua_adapter(return_type(T::*func)(arg_types...)) {
    return [=](void* obj, lua_State* L) {
        native_to_lua(L, call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename return_type, typename T, typename... arg_types>
lua_object_function lua_adapter(return_type(T::*func)(arg_types...) const) {
    return [=](void* obj, lua_State* L) {
        native_to_lua(L, call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    };
}

template <typename T, typename... arg_types>
lua_object_function lua_adapter(void(T::*func)(arg_types...)) {
    return [=](void* obj, lua_State* L) {
        call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <typename T, typename... arg_types>
lua_object_function lua_adapter(void(T::*func)(arg_types...) const) {
    return [=](void* obj, lua_State* L) {
        call_helper(L, (T*)obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    };
}

template <typename T>
lua_object_function lua_adapter(int(T::*func)(lua_State* L)) {
    return [=](void* obj, lua_State* L) {
        T* this_ptr = (T*)obj;
        return (this_ptr->*func)(L);
    };
}

template <typename T>
lua_object_function lua_adapter(int(T::*func)(lua_State* L) const) {
    return [=](void* obj, lua_State* L) {
        T* this_ptr = (T*)obj;
        return (this_ptr->*func)(L);
    };
}

using luna_member_wrapper = std::function<void(lua_State*, void*, char*)>;

int _lua_object_bridge(lua_State* L);

struct lua_export_helper {
    static luna_member_wrapper getter(const bool&) {
        return [](lua_State* L, void*, char* addr){ lua_pushboolean(L, *(bool*)addr); };
    }

    static luna_member_wrapper setter(const bool&) {
        return [](lua_State* L, void*, char* addr){ *(bool*)addr = lua_toboolean(L, -1); };
    }

    template <typename T>
    static typename std::enable_if<std::is_integral<T>::value, luna_member_wrapper>::type getter(const T&) {
        return [](lua_State* L, void*, char* addr){ lua_pushinteger(L, (lua_Integer)*(T*)addr); };
    }

    template <typename T>
    static typename std::enable_if<std::is_integral<T>::value, luna_member_wrapper>::type setter(const T&) {
        return [](lua_State* L, void*, char* addr){ *(T*)addr = (T)lua_tonumber(L, -1); };
    }    

    template <typename T>
    static typename std::enable_if<std::is_floating_point<T>::value, luna_member_wrapper>::type getter(const T&) {
        return [](lua_State* L, void*, char* addr){ lua_pushnumber(L, (lua_Number)*(T*)addr); };
    }

    template <typename T>
    static typename std::enable_if<std::is_floating_point<T>::value, luna_member_wrapper>::type setter(const T&) {
        return [](lua_State* L, void*, char* addr){ *(T*)addr = (T)lua_tonumber(L, -1); };
    }    

    static luna_member_wrapper getter(const std::string&) {
        return [](lua_State* L, void*, char* addr){
            const std::string& str = *(std::string*)addr;
            lua_pushlstring(L, str.c_str(), str.size()); 
        };
    }

    static luna_member_wrapper setter(const std::string&) {
        return [](lua_State* L, void*, char* addr){
            size_t len = 0;
            const char* str = lua_tolstring(L, -1, &len);
            if (str != nullptr) {
                *(std::string*)addr = std::string(str, len);                        
            }
        };
    }

    template <size_t Size>
    static luna_member_wrapper getter(const char (&)[Size]) {
        return [](lua_State* L, void*, char* addr){ lua_pushstring(L, addr);};
    }

    template <size_t Size>
    static luna_member_wrapper setter(const char (&)[Size]) {
        return [](lua_State* L, void*, char* addr){ 
            size_t len = 0;
            const char* str = lua_tolstring(L, -1, &len);
            if (str != nullptr && len < Size) {
                memcpy(addr, str, len);
                addr[len] = '\0';                    
            }
        };
    }
    
    template <typename return_type, typename T, typename... arg_types>
    static luna_member_wrapper getter(return_type(T::*func)(arg_types...)) {
        return [adapter=lua_adapter(func)](lua_State* L, void* obj, char*) mutable { 
            lua_pushlightuserdata(L, obj);
            lua_pushlightuserdata(L, &adapter);
            lua_pushcclosure(L, _lua_object_bridge, 2);
        };
    }

    template <typename return_type, typename T, typename... arg_types>
    static luna_member_wrapper setter(return_type(T::*func)(arg_types...)) {
        return [=](lua_State* L, void* obj, char*){ lua_rawset(L, -3); };
    }    

    template <typename return_type, typename T, typename... arg_types>
    static luna_member_wrapper getter(return_type(T::*func)(arg_types...) const) {
        return [adapter=lua_adapter(func)](lua_State* L, void* obj, char*) mutable { 
            lua_pushlightuserdata(L, obj);
            lua_pushlightuserdata(L, &adapter);
            lua_pushcclosure(L, _lua_object_bridge, 2);
        };
    }

    template <typename return_type, typename T, typename... arg_types>
    static luna_member_wrapper setter(return_type(T::*func)(arg_types...) const) {
        return [=](lua_State* L, void* obj, char*){ lua_rawset(L, -3); };
    }        
};

struct lua_member_item {
    const char* name;
    int offset;
    luna_member_wrapper getter;
    luna_member_wrapper setter;
};

template <typename T>
int lua_member_index(lua_State* L) {
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    const char* key = lua_tostring(L, 2);
    const char* meta_name = obj->lua_get_meta_name();
    if (key == nullptr || meta_name == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    luaL_getmetatable(L, meta_name);
    lua_pushstring(L, key);
    lua_rawget(L, -2);

    auto item = (lua_member_item*)lua_touserdata(L, -1);
    if (item == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 2);
    item->getter(L, obj, (char*)obj + item->offset);
    return 1;
}

template <typename T>
int lua_member_new_index(lua_State* L) {
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
    if (item == nullptr) {
        lua_rawset(L, -3);
        return 0;
    }

    if (item->setter) {
        item->setter(L, obj, (char*)obj + item->offset);
    }
    return 0;
}

template<typename T>
struct has_member_gc {
    template<typename U> static auto check_gc(int) -> decltype(std::declval<U>().__gc(), std::true_type());
    template<typename U> static std::false_type check_gc(...);
    enum { value = std::is_same<decltype(check_gc<T>(0)), std::true_type>::value };
};

#define MAX_LUA_OBJECT_KEY 256

template <typename T, int S>
const char* lua_get_object_key(char (&out)[S], T* obj) {
    const char* meta_name = obj->lua_get_meta_name();
    int len = snprintf(out, S, "%p@%s", obj, meta_name);
    if (len < 0 || len >= S) {
        return nullptr;
    }
    return out;
}

template <typename T>
int lua_object_gc(lua_State* L) {
    T* obj = lua_to_object<T*>(L, 1);
    if (obj == nullptr)
        return 0;

    char key[MAX_LUA_OBJECT_KEY];
    const char* pkey = lua_get_object_key(key, obj);
    if (pkey == nullptr)
        return 0;

    _lua_del_fence(L, pkey);

    if constexpr (has_member_gc<T>::value) {
        obj->__gc();
    } else {
        delete obj;
    }
    return 0;
}

template <typename T>
void lua_register_class(lua_State* L, T* obj) {
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

    while (item->name) {
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
void lua_push_object(lua_State* L, T obj) {
    if (obj == nullptr) {
        lua_pushnil(L);
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "__objects__");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);

        lua_newtable(L);
        lua_pushstring(L, "v");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);

        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "__objects__");
    }

    char key[MAX_LUA_OBJECT_KEY];
    const char* pkey = lua_get_object_key(key, obj);
    if (pkey == nullptr) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return;
    }

    // stack: __objects__
    if (lua_getfield(L, -1, pkey) != LUA_TTABLE) {
        if (!_lua_set_fence(L, pkey)) {
            lua_remove(L, -2);
            return;
        }

        lua_pop(L, 1);

        lua_newtable(L);
        lua_pushstring(L, "__pointer__");
        lua_pushlightuserdata(L, obj);
        lua_rawset(L, -3);

        // stack: __objects__, tab
        const char* meta_name = obj->lua_get_meta_name();
        luaL_getmetatable(L, meta_name);
        if (lua_isnil(L, -1)) {
            lua_remove(L, -1);
            lua_register_class(L, obj);
            luaL_getmetatable(L, meta_name);
        }
        lua_setmetatable(L, -2);

        // stack: __objects__, tab
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, pkey);
    }
    lua_remove(L, -2);
}

template <typename T>
void lua_detach(lua_State* L, T obj) {
   if (obj == nullptr)
        return;

    char key[MAX_LUA_OBJECT_KEY];
    const char* pkey = lua_get_object_key(key, obj);
    if (pkey == nullptr) {
        return;
    }

    _lua_del_fence(L, pkey);

    lua_getfield(L, LUA_REGISTRYINDEX, "__objects__");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // stack: __objects__
    if (lua_getfield(L, -1, pkey) != LUA_TTABLE) {
        lua_pop(L, 2);
        return;
    }

    // stack: __objects__, __shadow_object__
    lua_pushstring(L, "__pointer__");
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pushnil(L);
    lua_rawsetp(L, -3, obj);
    lua_pop(L, 2);
}

template<typename T>
struct has_meta_data {
    template<typename U> static auto check_meta(int) -> decltype(std::declval<U>().lua_get_meta_data(), std::true_type());
    template<typename U> static std::false_type check_meta(...);
    enum { value = std::is_same<decltype(check_meta<T>(0)), std::true_type>::value };
};

template <typename T>
T lua_to_object(lua_State* L, int idx) {
    T obj = nullptr;

    static_assert(has_meta_data<typename std::remove_pointer<T>::type>::value, "T should be declared export !");

    idx = lua_normal_index(L, idx);

    if (lua_istable(L, idx)) {
        lua_pushstring(L, "__pointer__");
        lua_rawget(L, idx);
        obj = (T)lua_touserdata(L, -1);
        lua_pop(L, 1);
    }
    return obj;
}

#define DECLARE_LUA_CLASS(ClassName)    \
    const char* lua_get_meta_name() { return "_class_meta:"#ClassName; }    \
    lua_member_item* lua_get_meta_data();

#define LUA_EXPORT_CLASS_BEGIN(ClassName)   \
lua_member_item* ClassName::lua_get_meta_data() { \
    using class_type = ClassName;  \
    static lua_member_item s_member_list[] = {

#define LUA_EXPORT_CLASS_END()    \
        { nullptr, 0, luna_member_wrapper(), luna_member_wrapper()}  \
    };  \
    return s_member_list;  \
}

#define LUA_EXPORT_PROPERTY_AS(Member, Name)   {Name, offsetof(class_type, Member), lua_export_helper::getter(((class_type*)nullptr)->Member), lua_export_helper::setter(((class_type*)nullptr)->Member)},
#define LUA_EXPORT_PROPERTY_READONLY_AS(Member, Name)   {Name, offsetof(class_type, Member), lua_export_helper::getter(((class_type*)nullptr)->Member), luna_member_wrapper()},
#define LUA_EXPORT_PROPERTY(Member)   LUA_EXPORT_PROPERTY_AS(Member, #Member)
#define LUA_EXPORT_PROPERTY_READONLY(Member)   LUA_EXPORT_PROPERTY_READONLY_AS(Member, #Member)

#define LUA_EXPORT_METHOD_AS(Method, Name) { Name, 0, lua_export_helper::getter(&class_type::Method), lua_export_helper::setter(&class_type::Method)},
#define LUA_EXPORT_METHOD_READONLY_AS(Method, Name) { Name, 0, lua_export_helper::getter(&class_type::Method), luna_member_wrapper()},
#define LUA_EXPORT_METHOD(Method) LUA_EXPORT_METHOD_AS(Method, #Method)
#define LUA_EXPORT_METHOD_READONLY(Method) LUA_EXPORT_METHOD_READONLY_AS(Method, #Method)

void lua_push_function(lua_State* L, lua_global_function func);
inline void lua_push_function(lua_State* L, lua_CFunction func) { lua_pushcfunction(L, func); }

template <typename T>
void lua_push_function(lua_State* L, T func) {
    lua_push_function(L, lua_adapter(func));
}

template <typename T>
void lua_register_function(lua_State* L, const char* name, T func) {
    lua_push_function(L, func);
    lua_setglobal(L, name);
}

inline bool lua_get_global_function(lua_State* L, const char function[]) {
    lua_getglobal(L, function);
    return lua_isfunction(L, -1);
}

bool lua_get_table_function(lua_State* L, const char table[], const char function[]);

template <typename T>
void lua_set_table_function(lua_State* L, int idx, const char name[], T func) {
    idx = lua_normal_index(L, idx);
    lua_push_function(L, func);
    lua_setfield(L, idx, name);
}

template <typename T>
bool lua_get_object_function(lua_State* L, T* object, const char function[]) {
    lua_push_object(L, object);
    if (!lua_istable(L, -1))
        return false;
    lua_getfield(L, -1, function);
    lua_remove(L, -2);
    return lua_isfunction(L, -1);
}

template<size_t... integers, typename... var_types>
void lua_to_native_mutil(lua_State* L, std::tuple<var_types&...>& vars, std::index_sequence<integers...>&&) {
    int _[] = { 0, (std::get<integers>(vars) = lua_to_native<var_types>(L, (int)integers - (int)sizeof...(integers)), 0)... };
}

bool lua_call_function(lua_State* L, std::string* err, int arg_count, int ret_count);

template <typename... ret_types, typename... arg_types>
bool lua_call_function(lua_State* L, std::string* err, std::tuple<ret_types&...>&& rets, arg_types... args) {
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    lua_pop(L,  (int)sizeof...(ret_types));
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_table_function(lua_State* L, std::string* err, const char table[], const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    lua_get_table_function(L, table, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    lua_pop(L,  (int)sizeof...(ret_types));
    return true;
}

template <typename T, typename... ret_types, typename... arg_types>
bool lua_call_object_function(lua_State* L, std::string* err, T* o, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    lua_get_object_function(L, o, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    lua_pop(L,  (int)sizeof...(ret_types));
    return true;
}

template <typename... ret_types, typename... arg_types>
bool lua_call_global_function(lua_State* L, std::string* err, const char function[], std::tuple<ret_types&...>&& rets, arg_types... args) {
    lua_getglobal(L, function);
    int _[] = { 0, (native_to_lua(L, args), 0)... };
    if (!lua_call_function(L, err, sizeof...(arg_types), sizeof...(ret_types)))
        return false;
    lua_to_native_mutil(L, rets, std::make_index_sequence<sizeof...(ret_types)>());
    lua_pop(L,  (int)sizeof...(ret_types));
    return true;
}

inline bool lua_call_table_function(lua_State* L, std::string* err, const char table[], const char function[]) { return lua_call_table_function(L, err, table, function, std::tie()); }
template <typename T> inline bool lua_call_object_function(lua_State* L, std::string* err, T* o, const char function[]) { return lua_call_object_function(L, err, o, function, std::tie()); }
inline bool lua_call_global_function(lua_State* L, std::string* err, const char function[]) { return lua_call_global_function(L, err, function, std::tie()); }

class lua_guard {
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
