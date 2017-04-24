--服务进程操作router的相关基础函数

addr_list =
{
    ["router1"] = {ip="127.0.0.1", port=8000},
    --["router2"] = {ip="127.0.0.1", port=8001},
};

routers = routers or {};
--router_name --> socket
connectings = connectings or {};

function select_router()
    current_router = nil;
    if #routers > 0 then
        current_router = routers[math.random(#routers)];
    end
end

function connect_router(name)
    local node = addr_list[name];
    local socket = socket_mgr.connect(node.ip, node.port);

    socket.name = name;

    socket.on_connected = function()
        --实例ID一般通过命令行参数设置,为了简便,这里就写死为1了
        connectings[name] = nil;
        routers[#routers + 1] = socket;
        socket.call("register", my_group, my_index);

        local connecting = false;
        for _name, _sock in pairs(connectings) do
            connecting = true;
            break;
        end

        if not connecting then
            select_router();
        end
    end

    socket.on_error = function (err)
        for i, s in ipairs(routers) do
            if s == socket then
                print("router connection lost, name="..name..", err="..err);
                table.remove(routers, i);
                select_router();
                return;
            end
        end
        print("failed to connect "..name);
        connectings[name] = nil;
    end

    socket.on_call = function (msg, ...)
        print(""..tostring(msg)..": ", ...);
    end

    routers[#routers + 1] = socket;
end

local groups = {router=1, gamesvr= 2, dbagent=3, matchsvr=5};

--group: 服务分组名,如"gamesvr", "dbagent", ...
--index: 实例编号
my_id = my_id or 0;
function setup(s_mgr, group, index)
    socket_mgr = s_mgr;
    my_group = groups[group];
    my_index = index;

    if not my_group then
        print("unknown service group: "..group);
        return;
    end

    my_id = my_group << 16 | my_index;

    for name, node in pairs(addr_list) do
        connect_router(name);
    end
end

function _G.call_dbagent(key, msg, ...)
    if current_router then
        current_router.forward_hash(groups.dbagent, key, msg, ...);
        return;
    end
    print("failed to call_dbagent, msg="..msg);
end

function _G.call_matchsvr(msg, ...)
    if current_router then
        current_router.forward_master(groups.matchsvr, msg, ...);
        return;
    end
    print("failed to call_matchsvr, msg="..msg);
end

local last_send_time = 0;
function send_heart_beat(now)
    if now - last_send_time < 1000 then
        return;
    end

    last_send_time = now;
    for i, socket in ipairs(routers) do
        socket.call("heart_beat");
    end
end

local last_check = 0;
function check_reconnect(now)
    if now - last_check < 5000 then
        return;
    end

    last_check = now;

    local connected = {};
    for i, socket in ipairs(routers) do
        connected[socket.name] = socket;
    end

    for name, node in pairs(addr_list) do
        if connected[name] == nil and connectings[name] == nil then
            connect_router(name);
        end
    end
end

function update(now)
    send_heart_beat(now);
    check_reconnect(now);
end
