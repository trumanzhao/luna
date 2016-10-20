#pragma once

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
		char* pbyData = GetData(&uDataLen);
		size_t uCopyLen = std::min(uDataLen, uSize);
		if (uCopyLen > 0)
		{
			char* pbyBuffer = new char[uSize];
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
		char* pbySpace = GetSpace(&uSpaceSize);
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

	char* GetSpace(size_t* puSize)
	{
		if (m_pbyBuffer == nullptr)
			Alloc();

		char* pbyEnd = m_pbyBuffer + m_uBufferSize;
		*puSize = (size_t)(pbyEnd - m_pbyDataEnd);
		return m_pbyDataEnd;
	}

	char* GetData(size_t* puSize)
	{
		*puSize = (size_t)(m_pbyDataEnd - m_pbyDataBegin);
		return m_pbyDataBegin;
	}

private:
	void Alloc()
	{
		m_pbyBuffer = new char[m_uBufferSize];
		m_pbyDataBegin = m_pbyBuffer;
		m_pbyDataEnd = m_pbyDataBegin;
	}

private:

	char* m_pbyDataBegin = nullptr;
	char* m_pbyDataEnd = nullptr;

	char* m_pbyBuffer = nullptr;
	size_t m_uBufferSize = 4096;
};
