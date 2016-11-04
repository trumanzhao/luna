run_flag = true;
_G.on_quit_signal = function(signo)
    print("on_quit_signal: "..signo);
    run_flag = false;
end

mgr = create_socket_mgr(100, 1024 * 1024, 1024 * 8);

listen = mgr.listen("", 8080);
server = nil;
listen.on_accept = function (stm)
    print("accept new connection ...");
    server = stm;

    server.on_error = function (err)
        print("server err: "..err);
    end

    server.on_recv = function (msg, a, b)
        print("msg="..msg..", a="..tostring(a)..", b="..tostring(b));
    end
end

client = mgr.connect("127.0.0.1", 8080);
if not client then
    print("client connect failed !");
end

client.on_connected = function ()
    print("client connected !");
    client.call("fuck", "you", 123);
end

client.on_error = function (err)
    print("client err: "..err);
end

frame = 0;
function on_frame(now)
    frame = frame + 1;
    if frame % 10 == 0 then
        print("frame="..frame);
    end
end

function _G.main()

    local last = 0;
    while run_flag do
        local now = get_time_ms();
        if now > last + 100 then
            last = now;
            on_frame(now);
            --pcall(on_frame, now);
        else
            mgr.wait(10);
            collectgarbage();
        end
    end
end

