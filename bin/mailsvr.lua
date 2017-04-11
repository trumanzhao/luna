
local on_connected = function ()
    socket.call("hello", 123);
	socket.call("fuck", 456);
end

local on_error = function (err)
    print("socket err: "..err);
end

local on_call = function (msg, ...)
    print(""..tostring(msg)..": ", ...);
    --socket.call("fuck", n + 1);
end

function on_loop(now)
    _G.luna_quit_flag = get_guit_signal();
    socket_mgr.wait(50);
end

function main()
	socket_mgr = luna.create_socket_mgr(100);
	socket = socket_mgr.connect("127.0.0.1", 7571);
	if not socket then
		print("connect failed !");
		return;
	end

	socket.on_connected = on_connected;
	socket.on_error = on_error;
	socket.on_call = on_call;

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
