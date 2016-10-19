-- repository: https://github.com/trumanzhao/luna
-- trumanzhao, 2016/10/14, trumanzhao@foxmail.com

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

function import(filename)
    local file_module = luna_files[filename];
    if file_module then
        return file_module.env;
    end

    local env = {};
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
