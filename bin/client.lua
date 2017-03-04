socket_mgr = create_socket_mgr(100);

client_socket = socket_mgr.connect("127.0.0.1", 7571);
if not client_socket then
    print("connect failed !");
    return;
end

client_socket.on_connected = function ()
    client_socket.call("hello", 123);
	client_socket.call("fuck", 456);
end

client_socket.on_error = function (err)
    print("client err: "..err);
end

client_socket.on_call = function (msg, ...)
    print(""..tostring(msg)..": ", ...);
    --client.call("fuck", n + 1);
end

function on_loop(now)
    luna_quit_flag = get_guit_signal();
    socket_mgr.wait(50);
end

