/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016/10/19, trumanzhao@foxmail.com
*/
#include <stddef.h>
#include <stdint.h>
#include "var_int.h"

size_t encode_u64(unsigned char* buffer, size_t buffer_size, uint64_t value)
{
    auto pos = buffer, end = buffer + buffer_size;
    do
    {
        if (pos >= end)
            return 0;
        auto code = (unsigned char)(value & 0x7F);
        value >>= 7;
        *pos++ = code | (value > 0 ? 0x80 : 0);
    } while (value > 0);
    return (size_t)(pos - buffer);
}

size_t encode_s64(unsigned char* buffer, size_t buffer_size, int64_t value)
{
    uint64_t uValue = (uint64_t)value;
    if (value < 0)
    {
        // 将符号位挪到最低位
        uValue--;
        uValue = ~uValue;
        uValue <<= 1;
        uValue |= 0x1;
    }
    else
    {
        uValue <<= 1;
    }
    return encode_u64(buffer, buffer_size, uValue);
}

size_t decode_u64(uint64_t* value, const unsigned char* data, size_t data_len)
{
    auto pos = data, end = data + data_len;
    uint64_t code = 0, number = 0;
    int bits = 0;
    // 在编码时,把数据按照7bit一组一组的编码,最多10个组,也就是10个字节
    // 第1组无需移位,第2组右移7位,第3组......,第10组(其实只有1位有效)右移了63位;
    // 所以,在解码的时候,最多左移63位就结束了:)
    while (true)
    {
        if (pos >= end || bits > 63)
            return 0;
        code = *pos & 0x7F;
        number |= (code << bits);
        if ((*pos++ & 0x80) == 0)
            break;
        bits += 7;
    }
    *value = number;
    return (size_t)(pos - data);
}

size_t decode_s64(int64_t* value, const unsigned char* data, size_t data_len)
{
    uint64_t number = 0;
    size_t count = decode_u64(&number, data, data_len);
    if (count == 0)
        return 0;

    if (number & 0x1)
    {
        number >>= 1;
        number = ~number;
        number++;
    }
    else
    {
        number >>= 1;
    }

    *value = (int64_t)number;
    return count;
}

