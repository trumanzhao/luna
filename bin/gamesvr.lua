import("group.lua");

--socket_mgr = ...
routers = routers or {}; --到多个router的socket连接数组

function select_router()
	local tab = {};
	for i, node in ipairs(routers) do
		if node.ok then
			tab[#tab] = node;
		end
	end
	
	routers.current = nil;
	if #tab > 0 then
		routers.current = tab[math.random(#tab)];
	end
end

function connect_router(ip, port)
	local socket = socket_mgr.connect(ip, port);
		
	socket.on_connected = function()
		--实例ID需要命令行设置,作为示例这里就写死为1了
		socket.call("register", GAME_SVR, 1);
		socket.ok = true;
		select_router();
	end
	
	socket.on_error = function (err)
		print("socket err: "..err);
		for i, node in ipairs(routers) do
			if node == socket then
				table.remove(routers, i);
				break;
			end
		end
		select_router();
		--如果这个router曾经连接成功过,那么重新连接
		--实际中可能需要延迟一会儿再尝试重连
		if socket.ok then
			connect_router(ip, port);
		end
	end
	
	socket.on_call = function (msg, ...)
		print(""..tostring(msg)..": ", ...);
		--socket.call("fuck", n + 1);
	end
	
	routers[#routers + 1] = socket;
end

function call_mailsvr(msg, ...)
	
end

function main()
	socket_mgr = luna.create_socket_mgr(100);

	connect_router("127.0.0.1", 7571);

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
end
