/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <locale>
#include <cstdint>
#include <signal.h>
#include "luna.h"
#include "tools.h"
#include "socket_io.h"
#include "socket_wapper.h"

lua_State* g_lvm = nullptr;
extern const char* luna_code;

static void on_quit_signal(int signo)
{
    lua_call_global_function(g_lvm, "on_quit_signal", std::tie(), signo);
}

#ifdef _MSC_VER
void daemon() {  } // do nothing !
#endif


int main(int argc, const char* argv[])
{
#if defined(__linux) || defined(__APPLE__)
    tzset();
#endif

#ifdef _MSC_VER
    _tzset();
#endif

    setlocale(LC_ALL, "");

    signal(SIGINT, on_quit_signal);
    signal(SIGTERM, on_quit_signal);

#if defined(__linux) || defined(__APPLE__)
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
#endif

    g_lvm = luaL_newstate();

    luaL_openlibs(g_lvm);
    int ret = luaL_dostring(g_lvm, luna_code);
    if (ret == 0)
    {
        lua_register_function(g_lvm, "get_file_time", get_file_time);
        lua_register_function(g_lvm, "get_time_ns", get_time_ns);
        lua_register_function(g_lvm, "get_time_ms", get_time_ms);
        lua_register_function(g_lvm, "sleep_ms", sleep_ms);
		lua_register_function(g_lvm, "daemon", daemon);
        lua_register_function(g_lvm, "create_socket_mgr", lua_create_socket_mgr);

        lua_guard g(g_lvm);
        lua_call_global_function(g_lvm, "luna_entry", std::tie(), argc > 1 ? argv[1] : "main.lua");
    }

    lua_close(g_lvm);
    return 0;
}

const char* luna_code =
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
	luna_files[filename] = { filename = filename, time = file_time, env = env };

	local ok, err = pcall(trunk);
	if not ok then
		print(err);
		return nil;
	end

	return env;
end

local try_reload = function()
	for filename, filenode in pairs(luna_files) do
		local filetime = get_file_time(filename);
		if filetime ~= 0 and filetime ~= filenode.time then
			filenode.time = filetime;
			local trunk = loadfile(filename, "bt", filenode.env);
			if  trunk then
				local ok, err = pcall(trunk);
				if not ok then
					print(err);
				end
			end
		end
	end
end

luna_quit_flag = false;
function on_quit_signal(sig_no)
	print("recv_quit_signal: "..tostring(sig_no));
	luna_quit_flag = true;
end

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
			try_reload();
			next_reload_time = now + 3000;
		end
	end
end
)---";
