socket_mgr = create_socket_mgr(100, 1024 * 1024, 1024 * 8);
listen_socket = socket_mgr.listen("127.0.0.1", 7571);

next_session_id = next_session_id or 1;
session_tab = session_tab or {};

listen_socket.on_accept = function(new_socket)
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

function on_loop(now)
    luna_quit_flag = get_guit_signal();
    socket_mgr.wait(50);
end
