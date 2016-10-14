--#!/usr/local/bin/lua
--repository: https://github.com/trumanzhao/misc
--trumanzhao, 2016/08/03, trumanzhao@foxmail.com
--[[
函数: log_tree(desc, var)
功能: 打印变量,特别是对table类型做树形输出
desc: 变量描述
var: 要输出的变量,可以是任何类型table, bool, number, nil
注意: 不保证性能,仅供调试用;对table中的多个key不保证输出顺序.
示例:
local player = {name="bitch", level=10, id=123};
log_tree("player_data", player);
--]]

local function var2string(var)
	local text = tostring(var);
	text = string.gsub(text, "\r", "\\r");
	text = string.gsub(text, "\n", "\\n");
	text = string.gsub(text, "%c", "\\^");
	return text;
end

local function tab2string(visited, path, base, tab)
    local pre = visited[tab];
    if pre then
        return pre;
    end

	visited[tab] = path;

	local size = 0;
    for k, v in pairs(tab) do
        size = size + 1;
    end

    if size == 0 then
        return "{ }";
    end

    local lines = {};
    local idx = 1;
	for k, v in pairs(tab) do
        local vtype = type(v);
        local header = base..(idx < size and "├─ " or "└─ ")..var2string(k);
        if vtype == "table" then
        	local vpath = visited[v];
        	if vpath then
        		lines[#lines + 1] = header..": "..vpath;
        	else
                local out = tab2string(visited, path.."/"..var2string(k), base..(idx < size and "│  " or "   "), v);
                if type(out) == "string" then
                    lines[#lines + 1] = header..": "..out;
                else
                    lines[#lines + 1] = header;
                    for _, one in ipairs(out) do
                        table.insert(lines, one);
                    end
                end
            end
        else
            local txt = var2string(v);
            if vtype == "string" then
                txt = '\"'..txt..'\"';
            end
            lines[#lines + 1] = header..": "..txt;
        end
        idx = idx + 1;
	end

    return lines;
end

function log_tree(desc, var)
    if type(var) ~= "table" then
        print(var2string(desc)..": "..var2string(var));
        return;
    end

    local visited = {};
    local out = tab2string(visited, "", "", var);

    if type(out) == "string" then
        print(var2string(desc)..": "..out);
        return;
    end

    print(var2string(desc));
    for i, line in ipairs(out) do
        print(line);
    end
end

