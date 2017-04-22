/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#include <stddef.h>

#define MAX_ENCODE_LEN  16

// 将整数编码为到变长字节数组:
// 返回值: 成功,返回变长编码长度; 失败,返回0;
size_t encode_u64(BYTE* buffer, size_t buffer_size, uint64_t value);
size_t encode_s64(BYTE* buffer, size_t buffer_size, int64_t value);

// 将变长字节序列解码为整数:
// 返回值: 成功,返回变长编码实际长度; 失败,返回0;
size_t decode_u64(uint64_t* value, const BYTE* data, size_t data_len);
size_t decode_s64(int64_t* value, const BYTE* data, size_t data_len);
