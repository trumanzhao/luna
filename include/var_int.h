/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#define MAX_VARINT_SIZE  16

// 将整数编码为到变长字节数组:
// 返回值: 成功,返回变长编码长度; 失败,返回0;
size_t encode_u64(unsigned char* buffer, size_t buffer_size, uint64_t value);
size_t encode_s64(unsigned char* buffer, size_t buffer_size, int64_t value);

// 将变长字节序列解码为整数:
// 返回值: 成功,返回变长编码实际长度; 失败,返回0;
size_t decode_u64(uint64_t* value, const unsigned char* data, size_t data_len);
size_t decode_s64(int64_t* value, const unsigned char* data, size_t data_len);
