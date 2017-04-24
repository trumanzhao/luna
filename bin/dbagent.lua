#!/usr/bin/luna
--dbagent示例

router_helper = import("router_helper.lua");

_G.s2s = s2s or {}; --所有server间的rpc定义在s2s中

function s2s.test(param)
    print("param");
end

function main()
	socket_mgr = luna.create_socket_mgr(100);

    router_helper.setup(socket_mgr, "dbagent", 1);

    local next_reload_time = 0;
	local quit_flag = false;
    while not quit_flag do
		socket_mgr.wait(50);

        local now = luna.get_time_ms();
        if now >= next_reload_time then
            luna.try_reload();
            next_reload_time = now + 3000;
        end

        router_helper.update(now);

        quit_flag = luna.get_guit_signal();
    end
end
