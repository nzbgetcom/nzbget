/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nzbget.h"
#include "NString.h"

template <int size>
BString<size>::BString(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	FormatV(format, ap);
	va_end(ap);
}

template <int size>
void BString<size>::Set(const char* str, int len)
{
	m_data[0] = '\0';
	int addLen = len > 0 ? std::min(size - 1, len) : size - 1;
	strncpy(m_data, str, addLen);
	m_data[addLen] = '\0';
}

template <int size>
void BString<size>::Append(const char* str, int len)
{
	if (len == 0)
	{
		len = strlen(str);
	}

	int curLen = strlen(m_data);
	int avail = size - curLen - 1;
	int addLen = std::min(avail, len);
	if (addLen > 0)
	{
		strncpy(m_data + curLen, str, addLen);
		m_data[curLen + addLen] = '\0';
	}
}

template <int size>
void BString<size>::AppendFmt(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	AppendFmtV(format, ap);
	va_end(ap);
}

template <int size>
void BString<size>::AppendFmtV(const char* format, va_list ap)
{
	int curLen = strlen(m_data);
	int avail = size - curLen;
	if (avail > 0)
	{
		vsnprintf(m_data + curLen, static_cast<size_t>(avail), format, ap);
	}
}

template <int size>
int BString<size>::Format(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	int len = FormatV(format, ap);
	va_end(ap);
	return len;
}

template <int size>
int BString<size>::FormatV(const char* format, va_list ap)
{
	// ensure result isn't negative (in case of bad argument)
	int len = std::max(vsnprintf(m_data, static_cast<size_t>(size), format, ap), 0);
	return len;
}

bool CString::operator==(const CString& other) const
{
	return (!m_data && !other.m_data) ||
		(m_data && other.m_data && !strcmp(m_data, other.m_data));
}

bool CString::operator==(const char* other) const
{
	return (!m_data && !other) ||
		(m_data && other && !strcmp(m_data, other));
}

void CString::Set(const char* str, int len)
{
	if (str)
	{
		if (len <= 0)
		{
			len = strlen(str);
		}
		char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(len + 1)));
		if (data)
		{
			m_data = data;
			strncpy(m_data, str, len);
			m_data[len] = '\0';
		}
	}
	else
	{
		free(m_data);
		m_data = nullptr;
	}
}

void CString::Append(const char* str, int len)
{
	if (len == 0)
	{
		len = strlen(str);
	}

	int curLen = Length();
	int newLen = curLen + len;
	char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(newLen + 1)));
	if (data)
	{
		m_data = data;
		strncpy(m_data + curLen, str, len);
		m_data[newLen] = '\0';
	}
}

void CString::AppendFmt(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	AppendFmtV(format, ap);
	va_end(ap);
}

void CString::AppendFmtV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int addLen = vsnprintf(nullptr, 0, format, ap);
	if (addLen <= 0)
	{
		va_end(ap2);
		return;
	}

	int curLen = Length();
	int newLen = curLen + addLen;
	char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(newLen + 1)));
	if (!data)
	{
		va_end(ap2);
		return;
	}

	if (vsnprintf(data + curLen, static_cast<size_t>(addLen + 1), format, ap2) > 0)
	{
		m_data = data;
	}
	else
	{
		free(data);
	}

	va_end(ap2);
}

int CString::Format(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	int len = FormatV(format, ap);
	va_end(ap);
	return len;
}

int CString::FormatV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int newLen = vsnprintf(nullptr, 0, format, ap);
	if (newLen < 0)
	{
		va_end(ap2);
		return 0;
	}

	newLen += 1; // for null-terminator
	char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(newLen)));
	if (!data)
	{
		va_end(ap2);
		return 0;
	}

	if (vsnprintf(data, static_cast<size_t>(newLen), format, ap2) > 0)
	{
		m_data = data;
		va_end(ap2);
		return newLen;
	}

	free(data);
	va_end(ap2);
	return 0;
}

int CString::Find(const char* str, int pos)
{
	if (pos != 0 && pos >= Length())
	{
		return -1;
	}
	char* res = strstr(m_data + pos, str);
	return res ? (int)(res - m_data) : -1;
}

void CString::Replace(int pos, int len, const char* str, int strLen)
{
	int addLen = strlen(str);
	if (strLen > 0)
	{
		addLen = std::min(addLen, strLen);
	}
	int curLen = Length();
	int delLen = pos + len <= curLen ? len : curLen - pos;
	int newLen = curLen - delLen + addLen;

	if (pos > curLen) return; // bad argument

	char* newvalue = static_cast<char*>(malloc(static_cast<size_t>(newLen + 1)));
	if (!newvalue) return;

	strncpy(newvalue, m_data, pos);
	strncpy(newvalue + pos, str, addLen);
	strcpy(newvalue + pos + addLen, m_data + pos + len);

	free(m_data);
	m_data = newvalue;
	m_data[newLen] = '\0';
}

void CString::Replace(const char* from, const char* to)
{
	int fromLen = strlen(from);
	int toLen = strlen(to);
	int pos = 0;
	while ((pos = Find(from, pos)) != -1)
	{
		Replace(pos, fromLen, to);
		pos += toLen;
	}
}

void CString::Bind(char* str)
{
	free(m_data);
	m_data = str;
}

char* CString::Unbind()
{
	char* olddata = m_data;
	m_data = nullptr;
	return olddata;
}

void CString::Reserve(int capacity)
{
	int curLen = Length();
	if (capacity > 0 && capacity > curLen || curLen == 0)
	{
		char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(capacity + 1)));
		if (data)
		{
			m_data = data;
			m_data[curLen] = '\0';
		}
	}
}

CString CString::FormatStr(const char* format, ...)
{
	CString result;
	va_list ap;
	va_start(ap, format);
	result.FormatV(format, ap);
	va_end(ap);
	return result;
}

void CString::TrimRight()
{
	int len = Length();

	if (len == 0)
	{
		return;
	}

	char* end = m_data + len - 1;
	while (end >= m_data && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
	{
		*end = '\0';
		end--;
	}
}


WString::WString(const char* utfstr)
{
	wchar_t* data = static_cast<wchar_t*>(malloc((strlen(utfstr) * 2 + 1) * sizeof(wchar_t)));
	if (!data) return;

	m_data = data;

	wchar_t* out = m_data;
	unsigned int codepoint = 0;
	while (*utfstr != 0)
	{
		unsigned char ch = (unsigned char)*utfstr;
		if (ch <= 0x7f)
			codepoint = ch;
		else if (ch <= 0xbf)
			codepoint = (codepoint << 6) | (ch & 0x3f);
		else if (ch <= 0xdf)
			codepoint = ch & 0x1f;
		else if (ch <= 0xef)
			codepoint = ch & 0x0f;
		else
			codepoint = ch & 0x07;
		++utfstr;
		if (((*utfstr & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
		{
			if (codepoint > 0xffff)
			{
				*out++ = (wchar_t)(0xd800 + (codepoint >> 10));
				*out++ = (wchar_t)(0xdc00 + (codepoint & 0x03ff));
			}
			else if (codepoint < 0xd800 || codepoint >= 0xe000)
				*out++ = (wchar_t)(codepoint);
		}
	}
	*out = '\0';
}


void StringBuilder::Clear()
{
	free(m_data);
	m_data = nullptr;
	m_length = 0;
	m_capacity = 0;
}

char* StringBuilder::Unbind()
{
	char* olddata = m_data;
	m_data = nullptr;
	m_length = 0;
	m_capacity = 0;
	return olddata;
}

void StringBuilder::Reserve(int capacity, bool exact)
{
	int oldCapacity = Capacity();
	if (capacity > oldCapacity || oldCapacity == 0)
	{
		char* oldData = m_data;

		// we may grow more than requested
		int smartCapacity = exact ? 0 : (int)(oldCapacity * 1.5);

		m_capacity = smartCapacity > capacity ? smartCapacity : capacity;

		char* data = static_cast<char*>(realloc(m_data, static_cast<size_t>(m_capacity + 1)));
		if (!data) return;

		m_data = data;
		if (!oldData)
		{
			m_data[0] = '\0';
		}
	}
}

void StringBuilder::Append(const char* str, int len)
{
	if (len == 0)
	{
		len = strlen(str);
	}
	int curLen = Length();
	int newLen = curLen + len;

	Reserve(newLen, false);

	strncpy(m_data + curLen, str, len);
	m_data[curLen + len] = '\0';
	m_length = newLen;
}

void StringBuilder::AppendFmt(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	AppendFmtV(format, ap);
	va_end(ap);
}

void StringBuilder::AppendFmtV(const char* format, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	int addLen = vsnprintf(nullptr, 0, format, ap);
	if (addLen <= 0)
	{
		va_end(ap2);
		return;
	}

	int curLen = Length();
	int newLen = curLen + addLen;

	Reserve(newLen, false);

	if (vsnprintf(m_data + curLen, static_cast<size_t>(newLen + 1), format, ap2) > 0)
	{
		m_length = newLen;
		m_data[newLen] = '\0';
	}

	va_end(ap2);
}


// Instantiate all classes used in our project
template class BString<1024>;
template class BString<100>;
template class BString<20>;
