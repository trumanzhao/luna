--#!/usr/local/bin/lua
--repository: https://github.com/trumanzhao/luna
--trumanzhao, 2016/11/05, trumanzhao@foxmail.com
--[[
--luna基础支持文件,本文件是luna运行的入口文件
--业务逻辑无需修改本文件
--]]

-- map<native_ptr, shadow_table>
luna_objects = luna_objects or {};
setmetatable(luna_objects, {__mode = "v"});

function luna_export(ptr, meta)
    local tab = luna_objects[ptr];
    if not tab then
        tab = {__pointer__=ptr};
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

luna_quit_flag = false;  --通过设置这个标志位退出程序

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
