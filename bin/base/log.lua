-- log_file = nil;
log_filename = log_filename or "out";
log_line_count = log_line_count or 0;
log_max_line = log_max_line or 100000;

function _G.log_open(filename, max_line)
    log_filename = filename;
    log_max_line = max_line or log_max_line;
end

function _G.log_close()
    if log_file then
        log_file:close();
        log_file = nil;
    end
end

function log_write(line)
    if log_line_count >= log_max_line then
        log_file:close();
        log_file = nil;
        log_line_count = 0;
    end

    if log_file == nil then
        local date = os.date("*t", os.time());
        local filename = string.format("%s_%02d%02d_%02d%02d%02d.log", log_filename, date.month, date.day, date.hour, date.min, date.sec);
        log_file = io.open(filename, "w");
        if log_file == nil then
            return;
        end
    end

    log_file:write(line);
    log_line_count = log_line_count + 1;
end

function _G.log_debug(fmt, ...)
    local line = string.format(fmt, ...);
    if line == nil then
        return;
    end

    log_write(string.format("DEBUG:\t%s\n", line));
end


function _G.log_info(fmt, ...)
    local line = string.format(fmt, ...);
    if line == nil then
        return;
    end

    log_write(string.format("INFO:\t%s\n", line));
end

function _G.log_err(fmt, ...)
    local line = string.format(fmt, ...);
    if line == nil then
        return;
    end

    log_write(string.format("ERROR:\t%s\n", line));
end
