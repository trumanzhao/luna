/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#include <assert.h>
#include <string.h>
#include <algorithm>
#include "lua/lua.hpp"
#include "lz4/lz4.h"
#include "lua_archiver.h"
#include "var_int.h"


enum class ar_type
{
    nill,
    number,
    integer,
    string,
    string_idx,
    bool_true,
    bool_false,
    table_head,
    table_tail,
    count
};

static const int small_int_max = UCHAR_MAX - (int)ar_type::count;
static const int max_share_string = UCHAR_MAX;
static const int max_table_depth = 16;

static int normal_index(lua_State* L, int idx) { return idx >= 0 ? idx : lua_gettop(L) + idx + 1; }

lua_archiver::lua_archiver(size_t size)
{
	m_buffer_size = size;
	m_lz_threshold = size;
	m_ar_buffer = new unsigned char[m_buffer_size];
	m_lz_buffer = new unsigned char[m_buffer_size];
}

lua_archiver::~lua_archiver()
{
	delete[] m_ar_buffer;
	delete[] m_lz_buffer;
}

void lua_archiver::set_buffer_size(size_t size)
{
	if (size > 0)
	{
		delete[] m_ar_buffer;
		m_ar_buffer = new unsigned char[size];
		delete[] m_lz_buffer;
		m_lz_buffer = new unsigned char[size];
		m_buffer_size = size;
	}
}

void* lua_archiver::save(size_t* data_len, lua_State* L, int first, int last)
{
	*m_ar_buffer = 'x';
	m_begin = m_ar_buffer;
	m_end = m_ar_buffer + m_buffer_size;
	m_pos = m_begin + 1;
	m_table_depth = 0;
	m_shared_string.clear();
	m_shared_strlen.clear();

	for (int i = first; i <= last; i++)
	{
		if (!save_value(L, i))
			return nullptr;
	}

	*data_len = (size_t)(m_pos - m_begin);

	if (*data_len >= m_lz_threshold && m_buffer_size < 1 + LZ4_COMPRESSBOUND(*data_len))
	{
		*m_lz_buffer = 'z';
		int len = LZ4_compress_default((const char*)m_begin + 1, (char*)m_lz_buffer + 1, (int)*data_len, (int)m_buffer_size - 1);
		if (len <= 0)
			return nullptr;
		*data_len = 1 + len;
		return m_lz_buffer;
	}

	return m_begin;
}

bool lua_archiver::load(int* param_count, lua_State* L, void* data, size_t data_len)
{
	if (data_len == 0)
		return false;

	m_pos = (unsigned char*)data;
	m_end = (unsigned char*)data + data_len;

	if (*m_pos == 'z')
	{
		m_pos++;
		int len = LZ4_decompress_safe((const char*)m_pos, (char*)m_lz_buffer, (int)data_len - 1, (int)m_buffer_size);
		if (len <= 0)
			return false;
		m_pos = m_lz_buffer;
		m_end = m_lz_buffer + len;
	}
	else
	{
		if (*m_pos != 'x')
			return false;
		m_pos++;
	}

	m_shared_string.clear();
	m_shared_strlen.clear();

	int count = 0;
	int top = lua_gettop(L);
	while (m_pos < m_end)
	{
		if (!load_value(L))
		{
			lua_settop(L, top);
			return false;
		}
		count++;
	}
	*param_count = count;
	return true;
}

bool lua_archiver::save_value(lua_State* L, int idx)
{
    int type = lua_type(L, idx);
    switch (type)
    {
    case LUA_TNIL:
        return save_nill();

    case LUA_TNUMBER:
        return lua_isinteger(L, idx) ? save_integer(lua_tointeger(L, idx)) : save_number(lua_tonumber(L, idx));

    case LUA_TBOOLEAN:
        return save_bool(!!lua_toboolean(L, idx));

    case LUA_TSTRING:
        return save_string(L, idx);

    case LUA_TTABLE:
        return save_table(L, idx);

    default:
        break;
    }
    return false;
}

bool lua_archiver::save_number(double v)
{
    if (m_end - m_pos < sizeof(unsigned char) + sizeof(double))
        return false;
    *m_pos++ = (unsigned char)ar_type::number;
    memcpy(m_pos, &v, sizeof(double));
    m_pos += sizeof(double);
    return true;
}

bool lua_archiver::save_integer(int64_t v)
{
    if (v >= 0 && v <= small_int_max)
    {
        if (m_end - m_pos < sizeof(unsigned char))
            return false;
        *m_pos++ = (unsigned char)(v + (int)ar_type::count);
        return true;
    }

    if (v > small_int_max)
    {
        v -= small_int_max;
    }

    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::integer;
    size_t len = encode_s64(m_pos, (size_t)(m_end - m_pos), v);
    m_pos += len;
    return len > 0;
}

bool lua_archiver::save_bool(bool v)
{
    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)(v ? ar_type::bool_true : ar_type::bool_false);
    return true;
}

bool lua_archiver::save_nill()
{
    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::nill;
    return true;
}

bool lua_archiver::save_table(lua_State* L, int idx)
{
    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;

    if (++m_table_depth > max_table_depth)
        return false;

    *m_pos++ = (unsigned char)ar_type::table_head;
    idx = normal_index(L, idx);

    lua_pushnil(L);
    while (lua_next(L, idx))
    {
        if (!save_value(L, -2) || !save_value(L, -1))
            return false;
        lua_pop(L, 1);
    }

    --m_table_depth;

    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;

    *m_pos++ = (unsigned char)ar_type::table_tail;
    return true;
}

bool lua_archiver::save_string(lua_State* L, int idx)
{
    size_t len = 0, encode_len = 0;
    const char* str = lua_tolstring(L, idx, &len);
    int shared = find_shared_str(str);
    if (shared >= 0)
    {
        if (m_end - m_pos < sizeof(unsigned char))
            return false;
        *m_pos++ = (unsigned char)ar_type::string_idx;
        encode_len = encode_u64(m_pos, (size_t)(m_end - m_pos), shared);
        m_pos += encode_len;
        return encode_len > 0;
    }

    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::string;

    encode_len = encode_u64(m_pos, (size_t)(m_end - m_pos), len);
    if (encode_len == 0)
        return false;
    m_pos += encode_len;

    if (m_end - m_pos < (int)len)
        return false;
    memcpy(m_pos, str, len);
    m_pos += len;

    if (m_shared_string.size() < max_share_string)
    {
        m_shared_string.push_back(str);
    }

    return true;
}

int lua_archiver::find_shared_str(const char* str)
{
    auto it = std::find(m_shared_string.begin(), m_shared_string.end(), str);
    if (it != m_shared_string.end())
        return (int)(it - m_shared_string.begin());
    return -1;
}

bool lua_archiver::load_value(lua_State* L)
{
    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;

    int code = *m_pos++;
    if (code >= (int)ar_type::count)
    {
        lua_pushinteger(L, code - (int)ar_type::count);
        return true;
    }

    double number = 0;
    int64_t integer = 0;
    size_t decode_len = 0;
    uint64_t str_len = 0, str_idx = 0;

    switch ((ar_type)code)
    {
    case ar_type::nill:
        lua_pushnil(L);
        break;

    case ar_type::number:
        if (m_end - m_pos < (ptrdiff_t)sizeof(double))
            return false;
        memcpy(&number, m_pos, sizeof(double));
        m_pos += sizeof(double);
        lua_pushnumber(L, number);
        break;

    case ar_type::integer:
        decode_len = decode_s64(&integer, m_pos, (size_t)(m_end - m_pos));
        if (decode_len == 0)
            return false;
        m_pos += decode_len;
        if (integer >= 0)
        {
            integer += small_int_max;
        }
        lua_pushinteger(L, integer);
        break;

    case ar_type::bool_true:
        lua_pushboolean(L, true);
        break;

    case ar_type::bool_false:
        lua_pushboolean(L, false);
        break;

    case ar_type::string:
        decode_len = decode_u64(&str_len, m_pos, (size_t)(m_end - m_pos));
        if (decode_len == 0)
            return false;
        m_pos += decode_len;
        if (m_end - m_pos < (ptrdiff_t)str_len)
            return false;
        m_shared_string.push_back((char*)m_pos);
        m_shared_strlen.push_back((size_t)str_len);
        lua_pushlstring(L, (char*)m_pos, (size_t)str_len);
        m_pos += str_len;
        break;

    case ar_type::string_idx:
        decode_len = decode_u64(&str_idx, m_pos, (size_t)(m_end - m_pos));
        if (decode_len == 0 || str_idx >= m_shared_string.size())
            return false;
        m_pos += decode_len;
        lua_pushlstring(L, m_shared_string[(int)str_idx], m_shared_strlen[(int)str_idx]);
        break;

    case ar_type::table_head:
        lua_newtable(L);
        while (m_pos < m_end)
        {
            if (*m_pos == (unsigned char)ar_type::table_tail)
            {
                m_pos++;
                return true;
            }
            if (!load_value(L) || !load_value(L))
                return false;
            lua_settable(L, -3);
        }
        return false;

    default:
        return false;
    }

    return true;
}
