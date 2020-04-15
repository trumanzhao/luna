/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#include <vector>

class lua_archiver {
public:
    lua_archiver(size_t size);
    lua_archiver(size_t ar_size, size_t lz_size);
    virtual ~lua_archiver();

    void set_buffer_size(size_t size);
    void set_lz_threshold(size_t size) { m_lz_threshold = size; }
    void set_max_array_reserve(int size) { m_max_arr_reserve = size; }
    void set_max_hash_reserve(int size) { m_max_hash_reserve = size; }

    void* save(size_t* data_len, lua_State* L, int first, int last);
    int load(lua_State* L, const void* data, size_t data_len);

private:
    bool alloc_buffer();
    void free_buffer();
    bool save_value(lua_State* L, int idx);
    bool save_number(double v);
    bool save_integer(int64_t v);
    bool save_bool(bool v);
    bool save_nil();
    bool save_table(lua_State* L, int idx);
    bool save_string(lua_State* L, int idx);
    int find_shared_str(const char* str);
    bool load_value(lua_State* L, bool tab_key);
    bool load_table(lua_State* L);

private:
    unsigned char* m_begin = nullptr;
    unsigned char* m_pos = nullptr;
    unsigned char* m_end = nullptr;
    int m_table_depth = 0;
    std::vector<const char*> m_shared_string;
    std::vector<size_t> m_shared_strlen;
    unsigned char* m_ar_buffer = nullptr;
    unsigned char* m_lz_buffer = nullptr;
    size_t m_ar_buffer_size = 0;
    size_t m_lz_buffer_size = 0;
    size_t m_lz_threshold = 0;
    int m_max_arr_reserve = 1024;
    int m_max_hash_reserve = 4096;
    int m_arr_reserve = 0;
    int m_hash_reserve = 0;
};
