
print("hello !");

function test()
    print("enter test");

    local a, b = get_fucker();

    print(a.add(1, 2));
    print(b.add(10, 20));

    local c = same_fucker(a);
    print(a.add(100, 200));

    print("leave test");
end

test();

print("ms: "..get_time_ms());
print("sleep: 10");
sleep_ms(10);
print("ms: "..get_time_ms());

run_flag = true;

_G.on_quit_signal = function(signo)
    print("on_quit_signal: "..signo);
    run_flag = false;
end

_G.main = function()
    while run_flag do
        sleep_ms(10);
    end
end

collectgarbage();
