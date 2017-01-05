/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#include <vector>

struct lua_archiver
{
    lua_archiver() {}
    ~lua_archiver() {}

    bool save(size_t* data_len, BYTE* buffer, size_t buffer_size, lua_State* L, int first, int last);
    bool load(int* param_count, lua_State* L, BYTE* data, size_t data_len);

private:
    bool save_value(lua_State* L, int idx);
    bool save_number(double v);
    bool save_integer(int64_t v);
    bool save_bool(bool v);
    bool save_nill();
    bool save_table(lua_State* L, int idx);
    bool save_string(lua_State* L, int idx);
    int find_shared_str(const char* str);
    bool load_value(lua_State* L);

private:
    BYTE* m_begin = nullptr;
    BYTE* m_pos = nullptr;
    BYTE* m_end = nullptr;
    int m_table_depth = 0;
    std::vector<const char*> m_shared_string;
    std::vector<size_t> m_shared_strlen;
};
