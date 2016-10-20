#pragma once

#include <assert.h>
#include <string.h>

struct XSocketBuffer
{
	XSocketBuffer() { }
	~XSocketBuffer() { Clear(); }

	void Clear()
	{
		if (m_pbyBuffer)
        {
            delete[] m_pbyBuffer;
            m_pbyBuffer = nullptr;
        }
		m_pbyDataBegin = nullptr;
		m_pbyDataEnd = nullptr;
	}

	void SetSize(size_t uSize)
	{
		if (uSize == m_uBufferSize)
			return;

		size_t uDataLen = 0;
		BYTE* pbyData = GetData(&uDataLen);
		size_t uCopyLen = uDataLen <= uSize ? uDataLen : uSize;
		if (uCopyLen > 0)
		{
			BYTE* pbyBuffer = new BYTE[uSize];
			memcpy(pbyBuffer, pbyData, uCopyLen);
			if (m_pbyBuffer)
            {
                delete[] m_pbyBuffer;
                m_pbyBuffer = nullptr;
            }
			m_pbyBuffer = pbyBuffer;
			m_pbyDataBegin = m_pbyBuffer;
			m_pbyDataEnd = m_pbyDataBegin + uCopyLen;
		}
		m_uBufferSize = uSize;
	}

	size_t GetSize() { return m_uBufferSize; }

	bool PushData(const void* pvData, size_t uDataLen)
	{
		if (m_pbyBuffer == nullptr)
			Alloc();

		size_t uSpaceSize = 0;
		BYTE* pbySpace = GetSpace(&uSpaceSize);
		if (uSpaceSize < uDataLen)
			return false;

		memcpy(pbySpace, pvData, uDataLen);
		PopSpace(uDataLen);

		return true;
	}

	void PopData(size_t uLen)
	{
		assert(m_pbyDataBegin + uLen <= m_pbyDataEnd);
		m_pbyDataBegin += uLen;
	}

	void PopSpace(size_t uLen)
	{
		assert(m_pbyDataEnd + uLen <= m_pbyBuffer + m_uBufferSize);
		m_pbyDataEnd += uLen;
	}

	void MoveDataToFront()
	{
		if (m_pbyDataBegin == m_pbyBuffer)
			return;

		size_t uLen = (size_t)(m_pbyDataEnd - m_pbyDataBegin);
		if (uLen > 0)
		{
			memmove(m_pbyBuffer, m_pbyDataBegin, uLen);
		}
		m_pbyDataBegin = m_pbyBuffer;
		m_pbyDataEnd = m_pbyDataBegin + uLen;
	}

	BYTE* GetSpace(size_t* puSize)
	{
		if (m_pbyBuffer == nullptr)
			Alloc();

		BYTE* pbyEnd = m_pbyBuffer + m_uBufferSize;
		*puSize = (size_t)(pbyEnd - m_pbyDataEnd);
		return m_pbyDataEnd;
	}

	BYTE* GetData(size_t* puSize)
	{
		*puSize = (size_t)(m_pbyDataEnd - m_pbyDataBegin);
		return m_pbyDataBegin;
	}

private:
	void Alloc()
	{
		m_pbyBuffer = new BYTE[m_uBufferSize];
		m_pbyDataBegin = m_pbyBuffer;
		m_pbyDataEnd = m_pbyDataBegin;
	}

private:

	BYTE* m_pbyDataBegin = nullptr;
	BYTE* m_pbyDataEnd = nullptr;

	BYTE* m_pbyBuffer = nullptr;
	size_t m_uBufferSize = 4096;
};
