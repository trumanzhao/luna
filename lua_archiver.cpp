/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#ifdef _MSC_VER
#include <intrin.h>
#include <winsock2.h>
#endif
#include "lua.hpp"
#include "lz4.h"
#include "lua_archiver.h"
#include "var_int.h"

#ifdef __linux
inline uint64_t htonll(uint64_t h64bits) { return htobe64(h64bits); }
inline uint64_t ntohll(uint64_t n64bits) { return be64toh(n64bits); }
#endif

enum class ar_type : unsigned char {
    nil,
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

static int normal_index(lua_State* L, int idx) {
    int top = lua_gettop(L);
    if (idx < 0 && -idx <= top)
        return idx + top + 1;
    return idx;
}

#if defined(__linux) || defined(__APPLE__)
inline unsigned long fast_log2(uint32_t x) {
    return x > 1 ? 32 - __builtin_clz(x - 1) : 0;
}
#endif

#ifdef _MSC_VER
inline unsigned long fast_log2(uint32_t x) {
    if (x > 1) {
        unsigned long idx = 0;
        _BitScanReverse(&idx, x - 1);
        return idx + 1;
    }
    return 0;
}
#endif

lua_archiver::lua_archiver(size_t size) {
    m_ar_buffer_size = size;
    m_lz_buffer_size = 1 + LZ4_COMPRESSBOUND(size - 1);
    m_lz_threshold = size / 2;
}

lua_archiver::lua_archiver(size_t ar_size, size_t lz_size) {
    m_ar_buffer_size = ar_size;
    m_lz_buffer_size = 1 + LZ4_COMPRESSBOUND(ar_size - 1);
    m_lz_threshold = lz_size;
}

lua_archiver::~lua_archiver() {
    free_buffer();
}

void lua_archiver::set_buffer_size(size_t size) {
    m_ar_buffer_size = size;
    m_lz_buffer_size = 1 + LZ4_COMPRESSBOUND(size - 1);
    free_buffer();
}

void* lua_archiver::save(size_t* data_len, lua_State* L, int first, int last) {
    first = normal_index(L, first);
    last = normal_index(L, last);
    if (last < first || !alloc_buffer())
        return nullptr;

    *m_ar_buffer = 'x';
    m_begin = m_ar_buffer;
    m_end = m_ar_buffer + m_ar_buffer_size;
    m_pos = m_begin + 1;
    m_table_depth = 0;
    m_shared_string.clear();
    m_shared_strlen.clear();

    for (int i = first; i <= last; i++) {
        if (!save_value(L, i))
            return nullptr;
    }

    *data_len = (size_t)(m_pos - m_begin);
    if (*data_len >= m_lz_threshold) {
        *m_lz_buffer = 'z';
        int raw_len = ((int)*data_len) - 1;        
        int out_len = LZ4_compress_default((const char*)m_begin + 1, (char*)m_lz_buffer + 1, raw_len, (int)m_lz_buffer_size - 1);
        if (out_len > 0) {
            *data_len = 1 + out_len;
            return m_lz_buffer;
        }
    }
    return m_ar_buffer;
}

int lua_archiver::load(lua_State* L, const void* data, size_t data_len) {
    if (data_len == 0 || !alloc_buffer())
        return 0;

    m_pos = (unsigned char*)data;
    m_end = (unsigned char*)data + data_len;

    if (*m_pos == 'z') {
        m_pos++;
        int len = LZ4_decompress_safe((const char*)m_pos, (char*)m_lz_buffer, (int)data_len - 1, (int)m_lz_buffer_size);
        if (len <= 0)
            return 0;
        m_pos = m_lz_buffer;
        m_end = m_lz_buffer + len;
    } else {
        if (*m_pos != 'x')
            return 0;
        m_pos++;
    }

    m_shared_string.clear();
    m_shared_strlen.clear();
    m_arr_reserve = m_max_arr_reserve;
    m_hash_reserve = m_max_hash_reserve;

    int count = 0;
    int top = lua_gettop(L);
    while (m_pos < m_end) {
        if (!load_value(L, false)) {
            lua_settop(L, top);
            return 0;
        }
        count++;
    }
    return count;
}

bool lua_archiver::alloc_buffer() {
    if (m_ar_buffer == nullptr) {
        m_ar_buffer = new unsigned char[m_ar_buffer_size];
    }

    if (m_lz_buffer == nullptr) {
        m_lz_buffer = new unsigned char[m_lz_buffer_size];
    }
    
    return m_ar_buffer != nullptr && m_lz_buffer != nullptr;
}

void lua_archiver::free_buffer() {
    if (m_ar_buffer) {
        delete[] m_ar_buffer;
        m_ar_buffer = nullptr;
    }

    if (m_lz_buffer) {
        delete[] m_lz_buffer;
        m_lz_buffer = nullptr;
    }
}

bool lua_archiver::save_value(lua_State* L, int idx) {
    int type = lua_type(L, idx);
    switch (type) {
    case LUA_TNIL:
        return save_nil();

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

bool lua_archiver::save_number(double v) {
    if (m_end - m_pos < sizeof(unsigned char) + sizeof(double))
        return false;
    *m_pos++ = (unsigned char)ar_type::number;
    uint64_t ni64 = htonll(*(uint64_t*)&v);
    memcpy(m_pos, &ni64, sizeof(ni64));
    m_pos += sizeof(ni64);
    return true;
}

bool lua_archiver::save_integer(int64_t v) {
    if (v >= 0 && v <= small_int_max) {
        if (m_end - m_pos < sizeof(unsigned char))
            return false;
        *m_pos++ = (unsigned char)(v + (int)ar_type::count);
        return true;
    }

    if (v > small_int_max) {
        v -= small_int_max;
    }

    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::integer;
    size_t len = encode_s64(m_pos, (size_t)(m_end - m_pos), v);
    m_pos += len;
    return len > 0;
}

bool lua_archiver::save_bool(bool v) {
    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)(v ? ar_type::bool_true : ar_type::bool_false);
    return true;
}

bool lua_archiver::save_nil() {
    if (m_end - m_pos < sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::nil;
    return true;
}

// table: table_head + lhsize + narr + (k,v)... + table_tail
bool lua_archiver::save_table(lua_State* L, int idx) {
    if (++m_table_depth > max_table_depth)
        return false;

    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char) * 2)
        return false;

    idx = normal_index(L, idx);
    *m_pos++ = (unsigned char)ar_type::table_head;
    unsigned char* lhsize = m_pos++;
    uint64_t narr = (uint64_t)luaL_len(L, idx);
    size_t encode_len = encode_u64(m_pos, (size_t)(m_end - m_pos), narr);
    if (encode_len == 0)
        return false;
    m_pos += encode_len;

    if (!lua_checkstack(L, 1))
        return false;

    int size = 0;
    lua_pushnil(L);
    while (lua_next(L, idx)) {
        if (!save_value(L, -2) || !save_value(L, -1))
            return false;
        ++size;
        lua_pop(L, 1);
    }

    // 考虑数组出现空洞的情况,narr未必是准确的,可能出现narr>size的情况
    // 由于这里的hsize只是作为load时预留之用,所以这种情况下记为0
    *lhsize = (unsigned char)fast_log2((unsigned)(size > narr ? size - narr : 0));

    --m_table_depth;

    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;
    *m_pos++ = (unsigned char)ar_type::table_tail;
    return true;
}

bool lua_archiver::save_string(lua_State* L, int idx) {
    size_t len = 0, encode_len = 0;
    const char* str = lua_tolstring(L, idx, &len);
    int shared = find_shared_str(str);
    if (shared >= 0) {
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

    if (m_shared_string.size() < max_share_string) {
        m_shared_string.push_back(str);
    }

    return true;
}

int lua_archiver::find_shared_str(const char* str) {
    auto it = std::find(m_shared_string.begin(), m_shared_string.end(), str);
    if (it != m_shared_string.end())
        return (int)(it - m_shared_string.begin());
    return -1;
}

bool lua_archiver::load_value(lua_State* L, bool tab_key) {
    if (!lua_checkstack(L, 1))
        return false;

    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;

    int code = *m_pos++;
    if (code >= (int)ar_type::count) {
        lua_pushinteger(L, code - (int)ar_type::count);
        return true;
    }

    size_t decode_len = 0;
    uint64_t str_len = 0, str_idx = 0;

    switch ((ar_type)code) {
    case ar_type::nil:
        if (tab_key)
            return false;
        lua_pushnil(L);
        break;

    case ar_type::number: {        
        if (m_end - m_pos < (ptrdiff_t)sizeof(int64_t))
            return false;
        uint64_t i64 = 0;       
        memcpy(&i64, m_pos, sizeof(i64));
        m_pos += sizeof(i64);        
        i64 = ntohll(i64);
        double f64 = *(double*)&i64;
        if (tab_key && isnan(f64))
            return false;
        lua_pushnumber(L, (lua_Number)f64);
        break;
    }

    case ar_type::integer: {
        int64_t integer = 0;
        decode_len = decode_s64(&integer, m_pos, (size_t)(m_end - m_pos));
        if (decode_len == 0)
            return false;
        m_pos += decode_len;
        if (integer >= 0) {
            integer += small_int_max;
        }
        lua_pushinteger(L, (lua_Integer)integer);
        break;
    }

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
        if (str_len > (uint64_t)(m_end - m_pos))
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
        return load_table(L);

    default:
        return false;
    }

    return true;
}

bool lua_archiver::load_table(lua_State* L) {
    if (m_end - m_pos < (ptrdiff_t)sizeof(unsigned char))
        return false;

    unsigned char lhsize = *m_pos++;
    uint64_t narr = 0;
    size_t decode_len = decode_u64(&narr, m_pos, (size_t)(m_end - m_pos));
    if (decode_len == 0)
        return false;
    m_pos += decode_len;

    uint64_t rest_len = (uint64_t)(m_end - m_pos);
    if (rest_len < 1)
        return false;

    uint64_t max_count = (rest_len - 1) / 2;
    if (narr > max_count)
        narr = 0;

    uint64_t hsize = (lhsize > 0 && lhsize < 31) ? (1ull << lhsize) : 0;
    if (hsize > max_count)
        hsize = max_count;

    int narr_i = (int)narr;
    if (m_max_arr_reserve >= 0) {
        if (narr_i > m_arr_reserve) {
            narr_i = m_arr_reserve;
        }
        m_arr_reserve -= narr_i;
    }

    int hsize_i = (int)hsize;
    if (m_max_hash_reserve >= 0) {
        if (hsize_i > m_hash_reserve) {
            hsize_i = m_hash_reserve;
        }
        m_hash_reserve -= hsize_i;
    }
    
    lua_createtable(L, narr_i, hsize_i);
    while (m_pos < m_end) {
        if (*m_pos == (unsigned char)ar_type::table_tail) {
            m_pos++;
            return true;
        }
        if (!load_value(L, true) || !load_value(L, false))
            return false;
        lua_settable(L, -3);
    }
    return false;
}

