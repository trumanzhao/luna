import("base/log.lua");

_G.s2s = {}; --所有server间的rpc定义在s2s中

--socket_mgr = luna.create_socket_mgr(...);
socket_list = socket_list or {};

--注册服务节点
--group: [0x00,0xFF]的整数,服务分组,比如所有gamesvr为一个组,所有mailsvr也是一个组
--index: [0x00,0xFFFF]的整数,表示服务节点在分组中的索引
function s2s.register(socket, group, index)
    local id = (group << 16) | (index & 0xffff);
    --socket_mgr会将同一个group的socket节点按照index排序成一个数组(token即代表了一个socket)
    --将id映射到0的token表示这个id位置保留,如果哈希或随机时遇到这个id,则会跳过它往后找,直到遇上非0的token或绕回
    --将id映射到nil的token表示将这个id从路由表中删除
    socket.id = id;
	socket.name = string.format("%02d:%04d", group, index);
    socket_mgr.route(id, socket.token);

    print("register: "..socket.name);

	--对于主从备份模式,需要设置master
	--master的选举逻辑可以自行设计的
	--socket_mgr.master(group, socket.token);
end

local on_call = function(socket, msg, ...)
    local proc = s2s[msg];
    if proc then
        proc(socket, ...);
        return;
    end
    log_err("remote call not exist: "..tostring(msg));
end

local on_error = function(socket, err)
    print("socket err: "..err);
    print("close session, name="..socket.name);

    for i, node in ipairs(socket_list) do
        if node == socket then
            table.remove(socket_list, i);
            break;
        end
    end

    if socket.id then
        --如果要实现固定哈希,则 route(socket.id, 0),当然,还得预先将所有的id注册
        socket_mgr.route(socket.id, nil);
    end
end

local on_accept = function(socket)
    print("accept new connection, ip="..socket.ip);
	socket.name = "unknown";
    socket_list[#socket_list + 1] = socket;
    socket.on_call = function(...) on_call(socket, ...); end
    socket.on_error = function(...) on_error(socket, ...); end
end

--为了支持热加载...
if listen_socket then
    listen_socket.on_accept = on_accept;
end

for i, socket in ipairs(socket_list) do
    socket.on_call = function(...) on_call(socket, ...); end
    socket.on_error = function(...) on_error(socket, ...); end
end

function main()
    log_open("router");
	socket_mgr = luna.create_socket_mgr(100, 1024 * 1024, 1024 * 8);
	listen_socket = socket_mgr.listen("127.0.0.1", 7571);
	listen_socket.on_accept = on_accept;

    local next_reload_time = 0;
	local quit_flag = false;
    while not quit_flag do
		quit_flag = luna.get_guit_signal();
		socket_mgr.wait(50);

        local now = luna.get_time_ms();
        if now >= next_reload_time then
            luna.try_reload();
            next_reload_time = now + 3000;
        end
    end
    log_close();
end
