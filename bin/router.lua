#!/usr/bin/luna
-- 路由转发示例:
-- router进程只做消息转发,不承担具体业务
-- 其他服务进程(gamesvr, dbagent, matchsvr...)通过router的包转发来实现相互通信
-- 同时,router也是实现集群中多进程分布的核心,通过路由设置,可以实现点对点,主从,哈希,随机,广播等消息转发模式
--在本实例中:
--多个gamesvr为对等关系,客户端可以任意连接一个
--多个dbagent提供数据库访问服务,访问时,按照哈希分布来访问数据库
--matchsvr提供全局战斗匹配服务,多个进程以主从备份的方式运行

_G.s2s = s2s or {}; --所有server间的rpc定义在s2s中

--socket_mgr = luna.create_socket_mgr(...);
socket_list = socket_list or {};

--注册服务节点,即: 标记socket所属的服务类型和实例编号
--group: [0x00,0xFF]的整数,服务分组,比如所有gamesvr为一个组,所有mailsvr也是一个组
--index: [0x00,0xFFFF]的整数,表示服务节点在分组中的索引
function s2s.register(socket, group, index)
    socket.id = group << 16 | index;
    --socket_mgr会将同一个group的socket节点按照index排序成一个数组(token即代表了一个socket)
    --将id映射到0表示这个id位置保留
    --将id映射到nil表示将这个id从路由表中删除
    --在进行哈希转发时,如果遇到token为0的id,则会跳过它往后找,直到遍历整个数组,随机转发也类似
    socket_mgr.route(socket.id, socket.token);
end

--设置自己为某个group的master
--关于master的选举,一个可选的方案是:
--将group的master进程的servcie_id记录到数据库中
--即记录: group --> {master=id, time=now}
--每个服务进程周期性的申请租约,如果该group没有master,或者master租约过期,或者master就是自己,那么就成功
--每次获取租约成功,都调用下面的set_master来更新master
--master之所以记录到数据库,是为了使得进程重启时不发生颠簸或错误,特别是多个router的情况下
--当然,router这里也可以加一段逻辑: 对多久没更新的master重置为0,master连接断开时也要响应的置0
function s2s.update_master(socket, group)
    socket_mgr.master(group, socket.token);
end

--心跳消息,略...
function s2s.heart_beat()
    -- body
end

local on_call = function(socket, msg, ...)
    local proc = s2s[msg];
    if proc then
        proc(socket, ...);
        return;
    end
    print("remote call not exist: "..tostring(msg));
end

local on_error = function(socket, err)
    print("socket err: "..err);

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
	socket_mgr = luna.create_socket_mgr(100, 1024 * 1024, 1024 * 8);
	listen_socket = socket_mgr.listen("127.0.0.1", 8000);
	listen_socket.on_accept = on_accept;

    local next_reload_time = 0;
	local quit_flag = false;
    while not quit_flag do
		socket_mgr.wait(50);

        local now = luna.get_time_ms();
        if now >= next_reload_time then
            luna.try_reload();
            next_reload_time = now + 3000;
        end

        quit_flag = luna.get_guit_signal();
    end
end
