/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
#include "Decoder.h"
#include "Log.h"
#include "Util.h"
#include "YEncDecryptor.h"
#include <rapidyenc.h>

// Non-raw yEnc decode: uses supported rapidyenc_decode_ex(is_raw=0)
// for restored encrypted blocks (which have already been dot-unstuffed).
static size_t RapidYencDecodeNoUnstuffing(const void* src, void* dest, size_t len, int* state)
{
	return rapidyenc_decode_ex(0, src, dest, len, reinterpret_cast<RapidYencDecoderState*>(state));
}

// Body Std v1.2 section 5: parse the applicable =yend CRC token for encrypted
// articles. The token runs to the first ASCII space, tab, CR or LF, or to the
// end of the bounded buffer; it must contain at least one ASCII hex digit,
// consume all its bytes (no sign or "0x" prefix, no trailing garbage), keep
// any width and any number of leading zeros, and not exceed 0xffffffff.
// Ordinary articles keep the lenient strtoul parsing in ProcessYenc.
static bool ParseEncryptedCrcToken(const char* token, const char* bufferEnd, uint32_t& value)
{
	uint64_t parsed = 0;
	bool anyDigit = false;
	while (token < bufferEnd && *token != ' ' && *token != '\t' && *token != '\r' && *token != '\n')
	{
		const char c = *token++;
		int digit;
		if (c >= '0' && c <= '9')
		{
			digit = c - '0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			digit = c - 'a' + 10;
		}
		else if (c >= 'A' && c <= 'F')
		{
			digit = c - 'A' + 10;
		}
		else
		{
			return false;
		}
		anyDigit = true;
		// Checked accumulation: parsed * 16 + digit must stay within UINT32_MAX.
		if (parsed > (UINT32_MAX - digit) / 16)
		{
			return false;
		}
		parsed = parsed * 16 + digit;
	}
	if (!anyDigit)
	{
		return false;
	}
	value = static_cast<uint32_t>(parsed);
	return true;
}

// Body Std v1.2 §5: on the encrypted path the =ybegin part= value enters
// the AAD as uint32, so it must be parsed with a checked conversion: at
// least one decimal digit, the entire token consumed, and the value must
// not exceed UINT32_MAX (>= 2^32 is rejected). The token runs to the first
// space, tab, CR, LF, NUL, or to the end of the bounded buffer. Ordinary
// articles keep the upstream atoi parsing in ProcessYenc.
static bool ParseEncryptedDecimalToken(const char* token, const char* bufferEnd, uint32_t& value)
{
	uint64_t parsed = 0;
	bool anyDigit = false;
	while (token < bufferEnd && *token != ' ' && *token != '\t' && *token != '\r' && *token != '\n')
	{
		const char c = *token++;
		if (c < '0' || c > '9')
		{
			return false;
		}
		anyDigit = true;
		const uint32_t digit = static_cast<uint32_t>(c - '0');
		if (parsed > (UINT32_MAX - digit) / 10)
		{
			return false;
		}
		parsed = parsed * 10 + digit;
	}
	if (!anyDigit)
	{
		return false;
	}
	value = static_cast<uint32_t>(parsed);
	return true;
}

Decoder::Decoder()
{
	Clear();
}

void Decoder::Clear()
{
	m_format = efUnknown;
	m_articleFilename.clear();
	m_rawArticleFilename.clear();
	m_partNumber = 1;
	m_body = false;
	m_begin = false;
	m_part = false;
	m_end = false;
	m_crc = false;
	m_eof = false;
	m_expectedCRC = 0;
	m_crc32.Reset();
	m_beginPos = 0;
	m_endPos = 0;
	m_size = 0;
	m_endSize = 0;
	m_outSize = 0;
	m_state = 0;
	m_crcCheck = false;
	m_lineBuf.Reserve(1024*8);
	m_lineBuf.SetLength(0);
	m_encrypted = false;
	m_authFailed = false;
	m_fatalError = false;
	// Reset the bootstrap-extracted segment index between articles — a stale
	// index from a previous article would corrupt this article's key derivation
	// and bypass dual-bootstrap agreement.
	m_segmentIndex = 0;
	memset(m_salt, 0, sizeof(m_salt));
	memset(m_tag, 0, sizeof(m_tag));
	m_cipherPayload.clear();
	m_decryptedPlaintext.clear();
	m_encryptedWireMode = false;
	m_wireProcessed = false;
	m_wireBuffer.clear();
	m_rawTail = "\r\n";
	m_wireTerminated = false;
	m_unstuffAtLineStart = true;
	m_unstuffPendingDot = false;
}

/* At the beginning of article the processing goes line by line to find '=ybegin'-marker.
 * Once the yEnc-data is started switches to blockwise processing.
 * At the end of yEnc-data switches back to line by line mode to
 * process '=yend'-marker and EOF-marker.
 * UU-encoded articles are processed completely in line by line mode.
 */
int Decoder::DecodeBuffer(char* buffer, int len, bool alreadyUnstuffed)
{
	if (m_rawMode)
	{
		ProcessRaw(buffer, len);
		return len;
	}

	// Control Std v1.2 §9 "Transport boundary": this decoder is the single
	// dot-unstuffing owner (RFC 3977 §3.1.1: strip exactly one line-start
	// escape dot; "\r\n.\r\n" is the terminator itself). Ordinary articles
	// are not unstuffed here — rapidyenc_decode_incremental does it in situ
	// — and the restored block from ProcessRestoredBlock went through
	// UnstuffInPlace when its chunks were accumulated, so such callers pass
	// alreadyUnstuffed=true: exactly one unstuffing pass per byte, ever.
	const bool wireCandidate = !m_wireProcessed &&
		(m_encryptedWireMode || (m_format == efUnknown && m_decryptor));
	if (wireCandidate && !alreadyUnstuffed)
	{
		TrackWireTerminator(buffer, len);
	}

	if (m_encryptedWireMode && !alreadyUnstuffed)
	{
		UnstuffInPlace(buffer, len);
	}

	// Check if candidate for encrypted wire mode
	if (wireCandidate)
	{
		// Bound line-1/wire staging to 16 MiB regardless of mode: a
		// newline-free or oversized stream must fail closed instead of
		// growing the buffer without limit.
		if (m_wireBuffer.size() + static_cast<size_t>(len) > MAX_WIRE_BUFFER_SIZE)
		{
			// Exceeding the local article-size bound is a local failure,
			// not corruption another provider could fix.
			m_fatalError = true;
			m_wireBuffer.clear();
			m_wireProcessed = true;
			m_eof = true;
			return 0;
		}

		m_wireBuffer.append(buffer, len);

		if (!m_encryptedWireMode)
		{
			size_t nlPos = m_wireBuffer.find('\n');
			if (nlPos != std::string::npos)
			{
				size_t line1End = nlPos;
				if (line1End > 0 && m_wireBuffer[line1End - 1] == '\r')
				{
					--line1End;
				}
				std::string line1 = m_wireBuffer.substr(0, line1End);

				// Combined-only reception (Body Std v1.2 §5, Control Std v1.2 §5 step 4):
				// an encrypted job whose Line 1 arrives as plaintext =ybegin/uue "begin"
				// is an unexpected fetched mode (body-only/unencrypted framing) —
				// provider corruption, never released as output.
				std::string candidateLine1 = line1;
				if (!candidateLine1.empty() && candidateLine1[0] == '.')
				{
					candidateLine1.erase(0, 1);
				}
				// An oversized Line 1 is rejected before FF1 (the cap and the
				// rationale live in YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE;
				// DecryptControlLine rejects it, this early check avoids
				// buffering pointless work): the FF1 round PRF works on the
				// whole line per round, so a multi-megabyte hostile line
				// would keep a download thread busy for hours (quadratic in
				// line length across the 10 Feistel rounds). The
				// Article-Bounded Memory clause (Control Std v1.2 §9) lets a
				// receiver enforce local limits before allocation or
				// decryption.
				if (candidateLine1.size() > YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE)
				{
					m_fatalError = true;
					m_wireBuffer.clear();
					m_wireProcessed = true;
					m_eof = true;
					return 0;
				}
				std::vector<uint8_t> ptLine1;
				std::vector<uint8_t> salt1;
				uint32_t extractedIndex = 0;
				auto st = m_decryptor->DecryptControlLine(
					reinterpret_cast<const uint8_t*>(candidateLine1.data()),
					candidateLine1.size(),
					0,
					1,
					true,
					ptLine1,
					&salt1,
					&extractedIndex
				);
				if (st == YEncDecryptor::Status::Ok && ptLine1.size() >= 8 && memcmp(ptLine1.data(), "=ybegin ", 8) == 0)
				{
					m_encryptedWireMode = true;
					m_format = efYenc;
					int wireLen = static_cast<int>(m_wireBuffer.size());
					UnstuffInPlace(m_wireBuffer.data(), wireLen);
					m_wireBuffer.resize(wireLen);
				}
				else
				{
					// A local crypto failure (sodium/Argon2id/oversized
					// line) is fatal; wire corruption stays failover.
					if (st == YEncDecryptor::Status::Error && m_decryptor->WasLastErrorFatal())
					{
						m_fatalError = true;
					}
					else
					{
						m_authFailed = true;
					}
					m_wireBuffer.clear();
					m_wireProcessed = true;
					m_eof = true;
					return 0;
				}
			}
		}

		if (m_encryptedWireMode)
		{
			// The terminator was located on the raw bytes, before unstuffing.
			if (m_wireTerminated)
			{
				ProcessRestoredBlock(m_wireBuffer);
				m_wireProcessed = true;
				m_eof = true;
			}
		}

		// Either the wire block is still being accumulated in m_wireBuffer
		// or line 1 has not arrived yet.
		return 0;
	}

	int outlen = 0;

	if (m_body && m_format == efYenc)
	{
		outlen = DecodeYenc(buffer, buffer, len);
		if (m_body)
		{
			return outlen;
		}
	}
	else
	{
		m_lineBuf.Append(buffer, len);
	}

	char* line = (char*)m_lineBuf;
	while (char* end = strchr(line, '\n'))
	{
		int llen = (int)(end - line + 1);

		if (line[0] == '.' && line[1] == '\r')
		{
			m_eof = true;
			m_lineBuf.SetLength(0);
			return outlen;
		}

		if (m_format == efUnknown)
		{
			m_format = DetectFormat(line, llen);
		}

		if (m_format == efYenc)
		{
			ProcessYenc(line, llen);
			if (m_body)
			{
				const char* next = end + 1;
				int rem = m_lineBuf.Length() - (int)(next - m_lineBuf);
				outlen = DecodeYenc(end + 1, buffer, rem);
				if (m_body)
				{
					m_lineBuf.SetLength(0);
					return outlen;
				}
				line = (char*)m_lineBuf;
				continue;
			}
		}
		else if (m_format == efUx)
		{
			outlen += DecodeUx(line, llen, buffer + outlen);
		}

		line = end + 1;
	}

	if (*line)
	{
		len = m_lineBuf.Length() - (int)(line - m_lineBuf);
		memmove((char*)m_lineBuf, line, len);
		m_lineBuf.SetLength(len);
	}
	else
	{
		m_lineBuf.SetLength(0);
	}

	return outlen;
}

void Decoder::TrackWireTerminator(const char* buffer, int len)
{
	// RFC 3977 section 3.1.1: the article ends with a line holding a single
	// dot. In the raw stream a data line that is a single dot is always sent
	// stuffed (".."), so an unstuffed ".\r\n" line cannot be data.
	m_rawTail.append(buffer, len);
	if (m_rawTail.ends_with("\r\n.\r\n") || m_rawTail.ends_with("\n.\n") || m_rawTail.ends_with("\n.\r\n"))
	{
		m_wireTerminated = true;
	}
	if (m_rawTail.size() > 4)
	{
		m_rawTail.erase(0, m_rawTail.size() - 4);
	}
}

void Decoder::UnstuffInPlace(char* buffer, int& len)
{
	int read = 0;
	int write = 0;

	// If the previous chunk ended exactly after a line-start '.', and this
	// chunk starts with '.', then the pair ".." was split across the chunk
	// boundary. Strip the second dot so only one dot is emitted in total.
	if (m_unstuffPendingDot)
	{
		m_unstuffPendingDot = false;
		if (read < len && buffer[read] == '.')
		{
			read++;
			m_unstuffAtLineStart = false;
		}
	}

	while (read < len)
	{
		if (m_unstuffAtLineStart && buffer[read] == '.')
		{
			if (read + 1 < len && buffer[read + 1] == '.')
			{
				// Stuffed data within chunk: ".." → "." (drop one dot).
				buffer[write++] = '.';
				read += 2;
			}
			else if (read + 2 < len && buffer[read + 1] == '\r' && buffer[read + 2] == '\n')
			{
				// Terminator within chunk: ".\r\n" — keep dot verbatim.
				buffer[write++] = buffer[read++];
				m_unstuffAtLineStart = false;
			}
			else if (read + 1 < len && buffer[read + 1] == '\n')
			{
				// LF-only terminator: ".\n" — keep dot verbatim.
				buffer[write++] = buffer[read++];
				m_unstuffAtLineStart = false;
			}
			else if (read + 2 == len && buffer[read + 1] == '\r')
			{
				// Chunk ends exactly at ".\r" (terminator in progress).
				// Keep both bytes verbatim so the terminator survives.
				buffer[write++] = buffer[read++];
				buffer[write++] = buffer[read++];
				m_unstuffAtLineStart = false;
			}
			else if (read + 1 == len)
			{
				// Chunk ends exactly at line-start '.'. Keep the dot
				// verbatim (it may be the article terminator ".\r\n" or ".\n",
				// or the first dot of a split stuffed pair ".."). Record
				// pending state so a following dot in the next chunk is stripped.
				buffer[write++] = buffer[read++];
				m_unstuffPendingDot = true;
				m_unstuffAtLineStart = false;
			}
			else
			{
				// Leading '.' followed by other content: the escape dot is stripped.
				read++;
			}
			m_unstuffAtLineStart = false;
			continue;
		}

		m_unstuffAtLineStart = buffer[read] == '\n';
		buffer[write++] = buffer[read++];
	}
	len = write;
}

void Decoder::ParseYpart(const char* buffer, const char* bufferEnd)
{
	m_part = true;
	m_body = true;
	// Body Std v1.2 §2.1 (encrypted path only): conflicting or malformed
	// duplicate begin=/end= fail closed as provider corruption. This must
	// run before any geometry value (including a range >= 2^32) is stored,
	// otherwise a 32-bit-truncated stored value could make the later
	// >= 2^32 check in CheckYenc pass silently. Ordinary yEnc never takes
	// this branch (m_encryptedWireMode false) and keeps first-match strstr.
	if (m_encryptedWireMode)
	{
		CheckEncryptedDuplicateGeometry(buffer, bufferEnd ? bufferEnd : buffer + strlen(buffer));
		if (m_authFailed)
		{
			return;
		}
	}
	const char* pb = strstr(buffer, " begin=");
	if (pb)
	{
		pb += 7; //=strlen(" begin=")
		m_beginPos = static_cast<int64>(atoll(pb));
	}
	pb = strstr(buffer, " end=");
	if (pb)
	{
		pb += 5; //=strlen(" end=")
		m_endPos = static_cast<int64>(atoll(pb));
	}
}

void Decoder::CheckEncryptedDuplicateGeometry(const char* buffer, const char* bufferEnd)
{
	static const char* const geometryKeywords[] = {"part=", "size=", "begin=", "end="};
	for (const char* keyword : geometryKeywords)
	{
		const size_t keywordLen = strlen(keyword);
		uint32_t resolved = 0;
		bool seen = false;
		// Scan whitespace-delimited tokens (the restored control line is
		// bounded by bufferEnd); only occurrences before any name= count,
		// so keyword-like text inside the file name is never a field.
		const char* cursor = buffer;
		while (cursor < bufferEnd)
		{
			while (cursor < bufferEnd && (*cursor == ' ' || *cursor == '\t'))
			{
				++cursor;
			}
			if (cursor >= bufferEnd)
			{
				break;
			}
			const char* tokenStart = cursor;
			while (cursor < bufferEnd && *cursor != ' ' && *cursor != '\t' &&
				*cursor != '\r' && *cursor != '\n')
			{
				++cursor;
			}
			if (cursor == tokenStart)
			{
				// The cursor sits on the line ending (e.g. "=ypart ...\r\n"
				// is followed by the restored body in the same buffer):
				// step over it so the token scan always makes progress.
				++cursor;
				break;
			}
			if (static_cast<size_t>(cursor - tokenStart) >= 5 &&
				strncmp(tokenStart, "name=", 5) == 0)
			{
				break;
			}
			if (static_cast<size_t>(cursor - tokenStart) > keywordLen &&
				strncmp(tokenStart, keyword, keywordLen) == 0)
			{
				uint32_t value = 0;
				if (!ParseEncryptedDecimalToken(tokenStart + keywordLen, cursor, value))
				{
					m_authFailed = true;
					m_decryptedPlaintext.clear();
					return;
				}
				if (!seen)
				{
					resolved = value;
					seen = true;
				}
				else if (value != resolved)
				{
					m_authFailed = true;
					m_decryptedPlaintext.clear();
					return;
				}
			}
		}
	}
}

void Decoder::ParseName(const char* buffer, const char* bufferEnd)
{
	const char* pb = buffer;
	pb += 6; //=strlen(" name=")
	const char* pe = pb;
	while (pe < bufferEnd && *pe != '\0' && *pe != '\n' && *pe != '\r')
	{
		// Encrypted wire mode: name= is the last =ybegin keyword and its value runs to end of line
		// (Control Std v1.2 section 3 and 5), so "=ypart " inside it is file name text, not a part marker.
		// Plain yEnc keeps the upstream scan for "=ypart ".
		if (!m_encryptedWireMode && pe + 7 <= bufferEnd && strncmp(pe, "=ypart ", 7) == 0)
		{
			ParseYpart(pe, bufferEnd);
			break;
		}
		++pe;
	}

	m_rawArticleFilename = std::string(pb, pe - pb);
	m_articleFilename = WebUtil::Latin1ToUtf8(CString(pb, (int)(pe - pb)));
}

Decoder::EFormat Decoder::DetectFormat(const char* buffer, int len)
{
	if (!strncmp(buffer, "=ybegin ", 8))
	{
		return efYenc;
	}

	if ((len == 62 || len == 63) && (buffer[62] == '\n' || buffer[62] == '\r') && *buffer == 'M')
	{
		return efUx;
	}

	if (!strncmp(buffer, "begin ", 6))
	{
		bool ok = true;
		buffer += 6; //strlen("begin ")
		while (*buffer && *buffer != ' ')
		{
			char ch = *buffer++;
			if (ch < '0' || ch > '7')
			{
				ok = false;
				break;
			}
		}
		if (ok)
		{
			return efUx;
		}
	}

	return efUnknown;
}

void Decoder::ProcessYenc(char* buffer, int len)
{
	// 1. Check =ybegin (Prefix length 8)
	if (len >= 8 && !strncmp(buffer, "=ybegin ", 8))
	{
		m_begin = true;
		// Body Std v1.2 §2.1 (encrypted path only): conflicting or malformed
		// duplicate part=/size= fail closed as provider corruption before
		// any value (including a size >= 2^32) is stored. Ordinary yEnc
		// never takes this branch and keeps first-match strstr/atoi.
		if (m_encryptedWireMode)
		{
			CheckEncryptedDuplicateGeometry(buffer, buffer + len);
			if (m_authFailed)
			{
				return;
			}
		}
		char* pb = strstr(buffer, " size=");
		if (pb && pb + 6 < buffer + len) // Ensure room for " size="
		{
			pb += 6;
			m_size = static_cast<int64>(atoll(pb));
		}
		
		char* partPtr = strstr(buffer, " part=");
		// Ensure marker is within the current buffer's length
		if (partPtr && partPtr >= buffer + len) partPtr = nullptr;
		// Encrypted wire mode (Body Std v1.2 section 2.1, Control Std v1.2 section 5): only keywords
		// before name= decide multipart; a " part=" inside the name value is not a part marker.
		// Plain yEnc keeps the upstream match above.
		if (partPtr && m_encryptedWireMode)
		{
			const char* namePtr = strstr(buffer, " name=");
			if (namePtr && namePtr < partPtr) partPtr = nullptr;
		}
		m_part = partPtr != nullptr;

		if (m_part && partPtr)
		{
			if (m_encryptedWireMode)
			{
				// Encrypted path (Body Std v1.2 §5): part= enters the AAD
				// as uint32, so parse it checked; a malformed or >= 2^32
				// value fails closed as provider corruption with no
				// plaintext released. Ordinary yEnc keeps atoi below.
				uint32_t encryptedPart = 0;
				if (!ParseEncryptedDecimalToken(partPtr + 6, buffer + len, encryptedPart))
				{
					m_authFailed = true;
					m_decryptedPlaintext.clear();
					return;
				}
				m_partNumber = static_cast<int>(encryptedPart);
			}
			else
			{
				m_partNumber = atoi(partPtr + 6);
			}
		}
		else
		{
			m_partNumber = 1;
		}

		if (!m_part)
		{
			m_body = true;
			m_beginPos = 1;
			m_endPos = m_size;
		}

		pb = strstr(buffer, " name=");
		if (pb && pb + 6 < buffer + len) // Ensure room for " name="
		{
			ParseName(pb, buffer + len);
		}
	}
	// 2. Check =ypart (Prefix length 7)
	else if (len >= 7 && !strncmp(buffer, "=ypart ", 7))
	{
		ParseYpart(buffer, buffer + len);
	}
	// 3. Check =yend (Prefix length 6)
	else if (len >= 6 && !strncmp(buffer, "=yend ", 6))
	{
		m_end = true;
		int crcOffset = 7 + static_cast<int>(m_part); // " crc32=" (7) or " pcrc32=" (8)
		char* pb = strstr(buffer, m_part ? " pcrc32=" : " crc32=");

		if (pb && pb + crcOffset < buffer + len)
		{
			m_crc = true;
			pb += crcOffset;
			// Body Std v1.2 section 5: encrypted articles must validate the
			// applicable CRC token fully (hex only, entire token consumed,
			// value <= 0xffffffff) before AEAD authentication. On an invalid
			// token fail closed as provider corruption without releasing any
			// plaintext; ordinary articles keep the lenient strtoul parsing.
			if (m_encrypted)
			{
				uint32_t encryptedCrc = 0;
				if (!ParseEncryptedCrcToken(pb, buffer + len, encryptedCrc))
				{
					m_authFailed = true;
					m_decryptedPlaintext.clear();
					return;
				}
				m_expectedCRC = encryptedCrc;
			}
			else
			{
				m_expectedCRC = static_cast<uint32>(strtoul(pb, nullptr, 16));
			}
		}
		
		pb = strstr(buffer, " size=");
		if (pb && pb + 6 < buffer + len)
		{
			pb += 6;
			m_endSize = atoll(pb);
		}
	}
}

int Decoder::DecodeYenc(char* buffer, char* outbuf, int len)
{
	const void* src = buffer;
	void* dst = outbuf;

	// In wire mode the restored block has already been dot-unstuffed
	// exactly once (UnstuffInPlace ran on the raw wire chunks). rapidyenc's
	// incremental decoder is hardwired isRaw and would strip every line-start
	// '.' a second time, corrupting restored yEnc data lines beginning with
	// '.'. For restored blocks, locate the "=y" footer marker ourselves and
	// decode the body with the non-raw decoder (no unstuffing); the "=yend"
	// line is then processed by the ordinary line loop. The restored block
	// contains no transport dot line: the article terminator was consumed on
	// the raw stream before restoration (TrackWireTerminator), and an interior
	// "." data line arrives stuffed as ".." and is restored verbatim, so a
	// dot scan here would truncate valid bodies. Ordinary articles keep the
	// raw incremental path (rapidyenc owns unstuffing in situ, including the
	// raw "\r\n.\r\n" EOF detection).
	if (m_encryptedWireMode)
	{
		size_t bodyLen = len;
		size_t markerOffset = len;
		if (len >= 2 && buffer[0] == '=' && buffer[1] == 'y')
		{
			bodyLen = 0;
			markerOffset = 0;
		}
		else
		{
			// No dot-terminator scan: the restored block holds no transport
			// dot line (see comment above); only "=y" ends the body here.
			for (size_t i = 0; i < (size_t)len; i++)
			{
				if (buffer[i] == '\n')
				{
					if (i + 2 < (size_t)len && buffer[i + 1] == '=' && buffer[i + 2] == 'y')
					{
						bodyLen = (i > 0 && buffer[i - 1] == '\r') ? i - 1 : i;
						markerOffset = i + 1;
						break;
					}
				}
			}
		}

		// Decode the body with the non-raw (no unstuffing) decoder. The non-raw
		// decoder consumes the entire bodyLen input in one call; the returned
		// count is bytes written to dst (escapes shrink the output).
		size_t bodyWritten = RapidYencDecodeNoUnstuffing(buffer, dst, bodyLen, &m_state);
		dst = (char*)dst + bodyWritten;

		int bytesWritten = static_cast<int>(static_cast<char*>(dst) - outbuf);

		if (markerOffset < (size_t)len)
		{
			// Body end found: switch back to line mode to process the
			// "=yend"/eof marker line (mirrors the raw endseq == 1 path).
			std::string marker(buffer + markerOffset, len - markerOffset);
			m_lineBuf.SetLength(0);
			m_lineBuf.Append(marker.data(), static_cast<int>(marker.size()));
			m_body = false;
		}

		if ((m_crcCheck || m_encrypted) && bytesWritten > 0)
		{
			m_crc32.Append(reinterpret_cast<unsigned char*>(outbuf), bytesWritten);
		}

		m_outSize += bytesWritten;

		if (m_encrypted)
		{
			m_cipherPayload.insert(m_cipherPayload.end(),
				reinterpret_cast<const uint8_t*>(outbuf),
				reinterpret_cast<const uint8_t*>(outbuf) + bytesWritten);
		}

		return bytesWritten;
	}

	auto endseq = rapidyenc_decode_incremental(&src, &dst, len, (RapidYencDecoderState*)&m_state);

	int bytesWritten = static_cast<int>(static_cast<char*>(dst) - outbuf);

	// endseq:
	//   0: no end sequence found
	//   1: \r\n=y sequence found, src points to byte after 'y'
	//   2: \r\n.\r\n sequence found, src points to byte after last '\n'
	if (endseq != 0) 
	{
		// switch back to line mode to process '=yend'- or eof- marker
		m_lineBuf.SetLength(0);
		m_lineBuf.Append(endseq == 1 ? "=y" : ".\r\n");
		int bytesConsumed = static_cast<int>(static_cast<const char*>(src) - buffer);
		int rem = len - bytesConsumed;
		if (rem > 0)
		{
			m_lineBuf.Append((const char*)src, rem);
		}
		m_body = false;
	}

	if (m_crcCheck)
	{
		m_crc32.Append(reinterpret_cast<unsigned char*>(outbuf), bytesWritten);
	}

	m_outSize += bytesWritten;

	return bytesWritten;
}

void Decoder::ProcessRestoredBlock(const std::string& wireBlock)
{
	if (!m_decryptor)
	{
		m_authFailed = true;
		return;
	}

	std::string cleanBlock;
	std::vector<uint8_t> line1Salt;
	YEncDecryptor::YEncryptionHeader header;
	YEncDecryptor::Status restoreStatus = m_decryptor->RestoreControlLines(
		wireBlock.data(), wireBlock.size(), cleanBlock, line1Salt, &header
	);
	if (restoreStatus != YEncDecryptor::Status::Ok)
	{
		if (restoreStatus == YEncDecryptor::Status::Error && m_decryptor->WasLastErrorFatal())
		{
			m_fatalError = true;
		}
		else
		{
			m_authFailed = true;
		}
		m_decryptedPlaintext.clear();
		return;
	}

	m_segmentIndex = header.segmentIndex;
	m_encrypted = true;
	memcpy(m_salt, header.salt.data(), 16);
	memcpy(m_tag, header.tag.data(), 16);

	// Reset state, then run the restored article through the ordinary decoder.
	// This keeps control-line restoration separate without duplicating the
	// line/body transition logic (especially the buffered =yend transition).
	m_format = efUnknown;
	m_begin = false;
	m_part = false;
	m_body = false;
	m_end = false;
	m_crc = false;
	m_eof = false;
	m_state = 0;
	m_expectedCRC = 0;
	m_crc32.Reset();
	m_outSize = 0;
	m_cipherPayload.clear();
	m_lineBuf.SetLength(0);
	m_wireProcessed = true;

	// cleanBlock's source chunks were unstuffed exactly once when
	// accumulated into m_wireBuffer; do not run a second unstuffing pass here
	// or restored yEnc data lines beginning with '.' get mangled.
	DecodeBuffer(cleanBlock.data(), static_cast<int>(cleanBlock.size()), /*alreadyUnstuffed=*/true);
}

Decoder::EStatus Decoder::Check()
{
	if (m_fatalError)
	{
		return dsFatalError;
	}

	if (m_authFailed)
	{
		return dsAuthFailed;
	}

	if (m_encryptedWireMode && !m_wireProcessed)
	{
		m_eof = true;
		ProcessRestoredBlock(m_wireBuffer);
		m_wireProcessed = true;
	}

	if (m_fatalError)
	{
		return dsFatalError;
	}

	if (m_authFailed)
	{
		return dsAuthFailed;
	}

	switch (m_format)
	{
		case efYenc:
			return CheckYenc();
			 
		case efUx:
			return CheckUx();

		default:
			return dsUnknownError;
	}
}

Decoder::EStatus Decoder::CheckYenc()
{
	m_calculatedCRC = m_crc32.Finish();

	debug("Expected crc32=%x", m_expectedCRC);
	debug("Calculated crc32=%x", m_calculatedCRC);

	if (m_fatalError)
	{
		return dsFatalError;
	}

	// Check auth failure before completeness so a truncated
	// encrypted article is classified as dsAuthFailed (provider failover
	// tier), not dsArticleIncomplete.
	if (m_authFailed)
	{
		return dsAuthFailed;
	}
	else if (m_encrypted && (m_size >= (int64(1) << 32) || m_beginPos >= (int64(1) << 32) || m_endPos >= (int64(1) << 32)))
	{
		m_authFailed = true;
		m_decryptedPlaintext.clear();
		return dsAuthFailed;
	}
	else if (!m_begin)
	{
		return dsNoBinaryData;
	}
	else if (m_encrypted && !m_end)
	{
		// Body Std v1.2 §5 PROVIDER_FAILOVER: truncated encrypted body is
		// provider corruption, not same-server incompleteness.
		m_authFailed = true;
		m_decryptedPlaintext.clear();
		return dsAuthFailed;
	}
	else if (!m_end)
	{
		return dsArticleIncomplete;
	}
	else if (m_encrypted && ((!m_part && m_size != m_endSize) || (m_endSize != m_outSize)))
	{
		m_authFailed = true;
		m_decryptedPlaintext.clear();
		return dsAuthFailed;
	}
	else if ((!m_part && m_size != m_endSize) || (m_endSize != m_outSize))
	{
		return dsInvalidSize;
	}
	else if (m_encrypted && !m_crc)
	{
		m_authFailed = true;
		m_decryptedPlaintext.clear();
		return dsAuthFailed;
	}
	else if ((m_crcCheck || m_encrypted) && m_crc && (m_expectedCRC != m_calculatedCRC))
	{
		if (m_encrypted)
		{
			// Force encrypted ciphertext CRC verification before AEAD.
			// Wire CRC mismatch fails authentication (provider corruption tier)
			// with zero plaintext output.
			m_authFailed = true;
			m_decryptedPlaintext.clear();
			return dsAuthFailed;
		}
		return dsCrcError;
	}

	if (m_encrypted)
	{
		if (m_segmentIndex == 0 || !m_decryptor)
		{
			m_authFailed = true;
			m_decryptedPlaintext.clear();
			return dsAuthFailed;
		}

		// AAD Subject (Body Standard Section 2.1): raw bytes of name parameter in =ybegin
		if (m_rawArticleFilename.empty())
		{
			m_authFailed = true;
			m_decryptedPlaintext.clear();
			return dsAuthFailed;
		}

		uint32_t partNumber = m_part ? static_cast<uint32_t>(m_partNumber) : 1U;
		uint32_t partBegin = m_part ? static_cast<uint32_t>(m_beginPos) : 1U;
		uint64_t segmentBytes = m_part ? static_cast<uint64_t>(m_endPos - m_beginPos + 1) : static_cast<uint64_t>(m_size);

		std::vector<uint8_t> aad = YEncDecryptor::BuildAad(m_rawArticleFilename, partNumber, partBegin, segmentBytes);
		if (aad.empty())
		{
			m_authFailed = true;
			m_decryptedPlaintext.clear();
			return dsAuthFailed;
		}

		m_decryptedPlaintext.clear();
		YEncDecryptor::Status st = m_decryptor->AuthenticateAndDecrypt(
			m_cipherPayload.data(), m_cipherPayload.size(),
			m_salt, m_tag, m_segmentIndex, aad.data(), aad.size(), m_decryptedPlaintext
		);
		if (st == YEncDecryptor::Status::Error)
		{
			// Local crypto failure (sodium/Argon2id/HMAC): fatal, no
			// provider can fix it. AuthFailed stays provider failover.
			m_fatalError = true;
			m_decryptedPlaintext.clear();
			return dsFatalError;
		}
		if (st != YEncDecryptor::Status::Ok)
		{
			m_authFailed = true;
			m_decryptedPlaintext.clear();
			return dsAuthFailed;
		}
	}

	return dsFinished;
}


/* DecodeUx-function uses portions of code from tool UUDECODE by Clem Dye
 * UUDECODE.c (http://www.bastet.com/uue.zip)
 * Copyright (C) 1998 Clem Dye
 *
 * Released under GPL (thanks)
 */

#define UU_DECODE_CHAR(c) (c == '`' ? 0 : (((c) - ' ') & 077))

int Decoder::DecodeUx(const char* inbuf, int len, char* outbuf)
{
	if (!m_body)
	{
		if (!strncmp(inbuf, "begin ", 6))
		{
			const char* pb = inbuf;
			pb += 6; //strlen("begin ")

			// skip file-permissions
			for (; *pb != ' ' && *pb != '\0' && *pb != '\n' && *pb != '\r'; pb++) ;
			pb++;

			// extracting filename
			const char* pe;
			for (pe = pb; *pe != '\0' && *pe != '\n' && *pe != '\r'; pe++) ;
			m_articleFilename = WebUtil::Latin1ToUtf8(CString(pb, static_cast<int>(pe - pb)));

			m_body = true;
			return 0;
		}
		else if ((len == 62 || len == 63) && (inbuf[62] == '\n' || inbuf[62] == '\r') && *inbuf == 'M')
		{
			m_body = true;
		}
	}

	if (m_body && (!strncmp(inbuf, "end ", 4) || *inbuf == '`'))
	{
		m_end = true;
	}

	if (m_body && !m_end)
	{
		int effLen = UU_DECODE_CHAR(inbuf[0]);
		if (effLen > len)
		{
			// error;
			return 0;
		}

		const char* iptr = inbuf;
		char* optr = outbuf;
		for (++iptr; effLen > 0; iptr += 4, effLen -= 3)
		{
			if (effLen >= 3)
			{
				*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4;
				*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2;
				*optr++ = UU_DECODE_CHAR (iptr[2]) << 6 | UU_DECODE_CHAR (iptr[3]);
			}
			else
			{
				*optr++ = UU_DECODE_CHAR (iptr[0]) << 2 | UU_DECODE_CHAR (iptr[1]) >> 4;
				if (effLen >= 2)
				{
					*optr++ = UU_DECODE_CHAR (iptr[1]) << 4 | UU_DECODE_CHAR (iptr[2]) >> 2;
				}
			}
		}

		return static_cast<int>(optr - outbuf);
	}

	return 0;
}

Decoder::EStatus Decoder::CheckUx()
{
	if (!m_body)
	{
		return dsNoBinaryData;
	}

	return dsFinished;
}

void Decoder::ProcessRaw(char* buffer, int len)
{
	switch (m_state)
	{
		case 1:
			m_eof = len >= 4 && buffer[0] == '\n' &&
				buffer[1] == '.' && buffer[2] == '\r' && buffer[3] == '\n';
			break;

		case 2:
			m_eof = len >= 3 && buffer[0] == '.' && buffer[1] == '\r' && buffer[2] == '\n';
			break;

		case 3:
			m_eof = len >= 2 && buffer[0] == '\r' && buffer[1] == '\n';
			break;

		case 4:
			m_eof = len >= 1 && buffer[0] == '\n';
			break;
	}

	m_eof |= std::string_view(buffer, len).find("\r\n.\r\n") != std::string_view::npos;

	if (len >= 4 && buffer[len-4] == '\r' && buffer[len-3] == '\n' &&
		buffer[len-2] == '.' && buffer[len-1] == '\r')
	{
		m_state = 4;
	}
	else if (len >= 3 && buffer[len-3] == '\r' && buffer[len-2] == '\n' && buffer[len-1] == '.')
	{
		m_state = 3;
	}
	else if (len >= 2 && buffer[len-2] == '\r' && buffer[len-1] == '\n')
	{
		m_state = 2;
	}
	else if (len >= 1 && buffer[len-1] == '\r')
	{
		m_state = 1;
	}
	else
	{
		m_state = 0;
	}
}
