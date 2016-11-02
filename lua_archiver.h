/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#include <vector>

class lua_archiver
{
private:
    lua_archiver();
    ~lua_archiver();

public:
	static lua_archiver* create(size_t buffer_size, size_t compress_threhold);

	void add_ref() { ++m_ref; }
	void release() { if (--m_ref == 0) delete this; }

	void resize(size_t buffer_size, size_t compress_threhold);

public:
	BYTE* save(size_t* data_len, lua_State* L, int first, int last);

private:
    bool save_value(lua_State* L, int idx);
    bool save_number(double v);
    bool save_integer(int64_t v);
    bool save_bool(bool v);
    bool save_nill();
    bool save_table(lua_State* L, int idx);
    bool save_string(lua_State* L, int idx);
    int find_shared_str(const char* str);

public:
    int load(lua_State* L, BYTE* data, size_t data_len);

private:
    bool load_value(lua_State* L);

private:
	int m_ref = 1;
	BYTE* m_buffer = nullptr;
	BYTE* m_compress = nullptr;
    size_t m_buffer_size = 0;
	BYTE* m_pos = nullptr;
	BYTE* m_end = nullptr;
    size_t m_compress_threhold = 4096;
    std::vector<const char*> m_shared_string;
    std::vector<size_t> m_shared_strlen;
};
