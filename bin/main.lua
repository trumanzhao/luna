next_session_id = next_session_id or 1;
session_tab = session_tab or {};

local on_accept = function(new_socket)
    print("accept new connection, ip="..new_socket.ip..", id="..next_session_id);

    session_tab[next_session_id] = new_socket;
    new_socket.id = next_session_id;
    next_session_id = next_session_id + 1;

    new_socket.on_error = function(err)
        print("socket err: "..err);
        print("close session, id="..new_socket.id);
        session_tab[new_socket.id] = nil;
    end

    new_socket.on_call = function(msg, ...)
        print(""..msg..": ", ...);
    end
end

function main()
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
end
