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

#define LUA_FILE_ENV_METATABLE  "__file_env_meta__"
#define LUA_FILE_ENV_PREFIX "__file:"
#define LUNA_RUNTIME_METATABLE  "__luna_runtime_meta__"
#define LUNA_RUNTIME_TABLE  "__luna_runtime__"

int Lua_object_bridge(lua_State* L)
{
	lua_pushstring(L, LUA_NATIVE_POINTER);
	lua_rawget(L, lua_upvalueindex(1));
	void* obj = lua_touserdata(L, -1);
	lua_object_function* func = (lua_object_function*)lua_touserdata(L, lua_upvalueindex(2));
	if (obj != nullptr && func != nullptr)
	{
		return (*func)(obj, L);
	}
	return 0;
}

static char* skip_utf8_bom(char* text, size_t len)
{
	if (len >= 3 && text[0] == (char)0xEF && text[1] == (char)0xBB && text[2] == (char)0xBF)
		return text + 3;
	return text;
}

static std::string regularize_path(const char path[])
{
	std::string str_path = path;
	std::replace(str_path.begin(), str_path.end(), '\\', '/');
	return str_path;
}

static bool get_file_time(time_t* mtime, const char file_name[])
{
	struct stat file_info;
	int ret = stat(file_name, &file_info);
	if (ret != 0)
		return false;

#ifdef __APPLE__
	*mtime = file_info.st_mtimespec.tv_sec;
#endif

#if defined(_MSC_VER) || defined(__linux)
	*mtime = file_info.st_mtime;
#endif
	return true;
}

static bool get_file_size(size_t* size, const char file_name[])
{
	struct stat info;
	int ret = stat(file_name, &info);
	if (ret != 0)
		return false;
	*size = (size_t)info.st_size;
	return true;
}

static bool read_file_data(char* buffer, size_t size, const char file_name[])
{
	FILE* file = fopen(file_name, "rb");
	if (file == nullptr)
		return false;
	size_t rcount = fread(buffer, size, 1, file);
	fclose(file);
	return (rcount == 1);
}

// runtime中预留了一些回调接口: 错误响应,获取文件时间,大小,数据
struct luna_runtime_t
{
	std::map<std::string, time_t> files;
	std::function<void(const char*)> error_func = [](const char* err) { puts(err); };
	std::function<bool(time_t*, const char*)> file_time_func = get_file_time;
	std::function<bool(size_t*, const char*)> file_size_func = get_file_size;
	std::function<bool(char*, size_t, const char*)> file_data_func = read_file_data;
	std::map<std::string, lua_global_function*> global_funcs;
};

static luna_runtime_t* get_luna_runtime(lua_State* L)
{
	lua_getglobal(L, LUNA_RUNTIME_TABLE);
	auto user_data = (luna_runtime_t**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return user_data ? *user_data : nullptr;
}

static void print_error(lua_State* L, const char* text)
{
	auto runtime = get_luna_runtime(L);
	runtime->error_func(text);
}

static int file_env_index(lua_State* L)
{
	const char* key = lua_tostring(L, 2);
	if (key != nullptr)
	{
		lua_getglobal(L, key);
	}
	else
	{
		lua_pushnil(L);
	}
	return 1;
}

static int lua_import(lua_State* L)
{
	int top = lua_gettop(L);
	const char* file_name = nullptr;
	std::string env_name = LUA_FILE_ENV_PREFIX;

	if (top != 1 || !lua_isstring(L, 1))
	{
		lua_pushnil(L);
		return 1;
	}

	file_name = lua_tostring(L, 1);
	env_name += regularize_path(file_name);

	lua_getglobal(L, env_name.c_str());
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		lua_load_script(L, file_name);
		lua_getglobal(L, env_name.c_str());
	}
	return 1;
}

static int luna_runtime_gc(lua_State* L)
{
	auto user_data = (luna_runtime_t**)lua_touserdata(L, 1);
	auto runtime = *user_data;
	for (auto one : runtime->global_funcs)
	{
		delete one.second;
	}
	delete runtime;
	*user_data = nullptr;
	return 0;
}

void lua_setup_env(lua_State* L)
{
	auto runtime = get_luna_runtime(L);
	if (runtime != nullptr)
		return;

	runtime = new luna_runtime_t();
	auto user_data = (luna_runtime_t**)lua_newuserdata(L, sizeof(runtime));
	*user_data = runtime;
	lua_pushvalue(L, -1);
	lua_setglobal(L, LUNA_RUNTIME_TABLE);

	luaL_newmetatable(L, LUNA_RUNTIME_METATABLE);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, luna_runtime_gc);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
	lua_pop(L, 1);

	luaL_newmetatable(L, LUA_FILE_ENV_METATABLE);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, file_env_index);
	lua_settable(L, -3);
	lua_pop(L, 1);

	lua_register(L, "import", lua_import);
}

static int Lua_global_bridge(lua_State* L)
{
	lua_global_function* func_ptr = (lua_global_function*)lua_touserdata(L, lua_upvalueindex(1));
	return (*func_ptr)(L);
}

void lua_register_function(lua_State* L, const char* name, lua_global_function func)
{
	auto runtime = get_luna_runtime(L);
	lua_global_function* func_ptr = nullptr;
	auto it = runtime->global_funcs.find(name);
	if (it != runtime->global_funcs.end())
	{
		func_ptr = it->second;
		*func_ptr = func;
		if (!func)
		{
			lua_pushnil(L);
			lua_setglobal(L, name);
		}
		return;
	}

	func_ptr = new lua_global_function(func);
	runtime->global_funcs[name] = func_ptr;
	lua_pushlightuserdata(L, func_ptr);
	lua_pushcclosure(L, Lua_global_bridge, 1);
	lua_setglobal(L, name);
}

bool lua_load_string(lua_State* L, const char env[], const char code[], int code_len)
{
	bool result = false;
	int top = lua_gettop(L);

	if (code_len == -1)
	{
		code_len = (int)strlen(code);
	}

	if (luaL_loadbuffer(L, code, code_len, env))
	{
		print_error(L, lua_tostring(L, -1));
		goto exit0;
	}

	lua_getglobal(L, env);
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);

		// file env table
		lua_newtable(L);

		luaL_getmetatable(L, LUA_FILE_ENV_METATABLE);
		lua_setmetatable(L, -2);

		lua_pushvalue(L, -1);
		lua_setglobal(L, env);
	}
	lua_setupvalue(L, -2, 1);

	if (lua_pcall(L, 0, 0, 0))
	{
		print_error(L, lua_tostring(L, -1));
		goto exit0;
	}

	result = true;
exit0:
	lua_settop(L, top);
	return result;
}

bool lua_load_script(lua_State* L, const char file_name[])
{
	bool result = false;
	auto runtime = get_luna_runtime(L);
	std::string file_path = regularize_path(file_name);
	std::string env_name = LUA_FILE_ENV_PREFIX;
	time_t file_time = 0;
	size_t file_size = 0;
	char* buffer = nullptr;
	char* code = nullptr;

	env_name += file_path;

	if (!runtime->file_time_func(&file_time, file_name))
		goto exit0;

	if (!runtime->file_size_func(&file_size, file_name))
		goto exit0;

	buffer = new char[file_size];
	if (buffer == nullptr)
		goto exit0;

	if (!runtime->file_data_func(buffer, file_size, file_name))
		goto exit0;

	code = skip_utf8_bom(buffer, file_size);
	if (lua_load_string(L, env_name.c_str(), code, (int)(buffer + file_size - code)))
	{
		runtime->files[file_path] = file_time;
		result = true;
	}
exit0:
	delete[] buffer;
	return result;
}

void lua_reload_update(lua_State* L)
{
	auto runtime = get_luna_runtime(L);
	for (auto& one : runtime->files)
	{
		const char* file_name = one.first.c_str();
		time_t new_time = 0;
		if (runtime->file_time_func(&new_time, file_name))
		{
			if (new_time != one.second)
			{
				lua_load_script(L, file_name);
			}
		}
	}
}

bool lua_get_file_function(lua_State* L, const char file_name[], const char function[])
{
	bool result = false;
	int top = lua_gettop(L);
	std::string env_name = LUA_FILE_ENV_PREFIX;

	env_name += regularize_path(file_name);
	lua_getglobal(L, env_name.c_str());

	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		if (!lua_load_script(L, file_name))
			goto Exit0;

		lua_getglobal(L, env_name.c_str());
	}
	lua_getfield(L, -1, function);
	lua_remove(L, -2);
	result = lua_isfunction(L, -1);
Exit0:
	if (!result)
	{
		lua_settop(L, top);
	}
	return result;
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
		print_error(L, "call invalid function !");
		return false;
	}

	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_remove(L, -2); // remove 'debug'

	lua_insert(L, func_idx);
	if (lua_pcall(L, arg_count, ret_count, func_idx))
	{
		print_error(L, lua_tostring(L, -1));
		return false;
	}
	lua_remove(L, -ret_count - 1); // remove 'traceback'
	return true;
}

void lua_set_error_func(lua_State* L, std::function<void(const char*)>& error_func)
{
	auto runtime = get_luna_runtime(L);
	runtime->error_func = error_func;
}

void lua_set_file_time_func(lua_State* L, std::function<bool(time_t*, const char*)>& time_func)
{
	auto runtime = get_luna_runtime(L);
	runtime->file_time_func = time_func;
}

void lua_set_file_size_func(lua_State* L, std::function<bool(size_t*, const char*)>& size_func)
{
	auto runtime = get_luna_runtime(L);
	runtime->file_size_func = size_func;
}

void lua_set_file_data_func(lua_State* L, std::function<bool(char*, size_t, const char*)>& data_func)
{
	auto runtime = get_luna_runtime(L);
	runtime->file_data_func = data_func;
}
