/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include <assert.h>
#include <limits.h>
#include <string.h>

struct io_buffer
{
    io_buffer() { }
    ~io_buffer() { SAFE_DELETE_ARRAY(m_buffer); }

    void resize(size_t size)
    {
        size_t data_len = 0;
        BYTE* data = peek_data(&data_len);
        if (size == m_buffer_size || size < data_len)
            return;

        if (data_len > 0)
        {
            BYTE* pbyBuffer = new BYTE[size];
            memcpy(pbyBuffer, data, data_len);
            delete[] m_buffer;
            m_buffer = pbyBuffer;
            m_data_begin = m_buffer;
            m_data_end = m_data_begin + data_len;
        }
        else
        {
            // 这里只释放而不分配新的缓冲区,在需要用到的时候懒惰分配
            if (m_buffer != nullptr)
            {
                delete[] m_buffer;
                m_buffer = nullptr;
            }
        }
        m_buffer_size = size;
    }

    bool push_data(const void* data, size_t data_len)
    {
        if (m_buffer == nullptr)
            alloc_buffer();

        size_t space_len = 0;
        auto* space = peek_space(&space_len);
        if (space_len < data_len)
            return false;

        memcpy(space, data, data_len);
        m_data_end += data_len;

        return true;
    }

    void pop_data(size_t uLen)
    {
        assert(m_data_begin + uLen <= m_data_end);
        m_data_begin += uLen;
    }

    void regularize(bool try_free = false)
    {
        size_t data_len = (size_t)(m_data_end - m_data_begin);
        if (data_len > 0)
        {
            if (m_data_begin > m_buffer)
            {
                memmove(m_buffer, m_data_begin, data_len);
                m_data_begin = m_buffer;
                m_data_end = m_data_begin + data_len;
            }
        }
        else
        {
            if (try_free && m_buffer != nullptr)
            {
                delete[] m_buffer;
                m_data_begin = m_data_end = m_buffer = nullptr;
            }
        }
    }

    void clear(bool with_free = false)
    {
        if (with_free)
        {
            SAFE_DELETE_ARRAY(m_buffer);
        }
        m_data_begin = m_buffer;
        m_data_end = m_data_begin;
    }

    BYTE* peek_space(size_t* len)
    {
        if (m_buffer == nullptr)
            alloc_buffer();

        auto buffer_end = m_buffer + m_buffer_size;
        *len = (size_t)(buffer_end - m_data_end);
        return m_data_end;
    }

    void pop_space(size_t pop_len)
    {
        if (m_buffer == nullptr)
            alloc_buffer();

        assert(m_data_end + pop_len <= m_buffer + m_buffer_size);
        m_data_end += pop_len;
    }

    BYTE* pop_space(size_t* space_len, size_t pop_len)
    {
        if (m_buffer == nullptr)
            alloc_buffer();

        auto buffer_end = m_buffer + m_buffer_size;
        if (m_data_end + pop_len > buffer_end)
            return nullptr;

        m_data_end += pop_len;
        if (space_len)
        {
            *space_len = (size_t)(buffer_end - m_data_end);
        }
        return m_data_end;
    }

    BYTE* peek_data(size_t* data_len)
    {
        *data_len = (size_t)(m_data_end - m_data_begin);
        return m_data_begin;
    }

    bool empty() { return m_data_end <= m_data_begin; }

private:
    void alloc_buffer()
    {
        m_buffer = new BYTE[m_buffer_size];
        m_data_begin = m_buffer;
        m_data_end = m_data_begin;
    }

    BYTE* m_data_begin = nullptr;
    BYTE* m_data_end = nullptr;
    BYTE* m_buffer = nullptr;
    size_t m_buffer_size = USHRT_MAX;
};
