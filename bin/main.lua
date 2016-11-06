mgr = create_socket_mgr(100, 1024 * 1024, 1024 * 8);

listen = mgr.listen("", 8080);
server = nil;
listen.on_accept = function (stm)
    print("accept new connection, ip="..stm.ip);
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
    print("client connected, remote="..client.ip);
    client.call("fuck", "you", 123);
end

client.on_error = function (err)
    print("client err: "..err);
end

local frame = 0;
function on_frame(now)
    frame = frame + 1;
    if frame % 10 == 0 then
        print("frame="..frame);
    end
end

local next_frame_time = 0;
local next_gc_time = 0;

function on_loop(now)
    if now >= next_frame_time then
        next_frame_time = now + 100;
        on_frame(now);
    end

    if now >= next_gc_time then
        collectgarbage();
        next_gc_time = now + 500;
    end

    mgr.wait(50);
end

