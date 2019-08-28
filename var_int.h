/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/

#pragma once

#define MAX_VARINT_SIZE  16

// 将无符号整数编码到字节数组
// 返回值: 成功,返回编码长度; 失败,返回0;
size_t encode_u64(unsigned char* buffer, size_t buffer_size, uint64_t value);
// 从字节数组解码无符号整数
// 返回值: 成功,返回解码长度; 失败,返回0;
size_t decode_u64(uint64_t* value, const unsigned char* data, size_t data_len);

// 将有符号整数编码到字节数组
// 返回值: 成功,返回编码长度; 失败,返回0;
size_t encode_s64(unsigned char* buffer, size_t buffer_size, int64_t value);
// 从字节数组解码有符号整数
// 返回值: 成功,返回解码长度; 失败,返回0;
size_t decode_s64(int64_t* value, const unsigned char* data, size_t data_len);
