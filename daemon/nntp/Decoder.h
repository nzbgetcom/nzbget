/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef DECODER_H
#define DECODER_H

#include <string>
#include <vector>
#include <cstdint>
#include "NString.h"
#include "Util.h"

class YEncDecryptor;

class Decoder
{
public:
	enum EStatus
	{
		dsUnknownError,
		dsFinished,
		dsArticleIncomplete,
		dsCrcError,
		dsInvalidSize,
		dsNoBinaryData,
		dsAuthFailed,
		// Local crypto failure (sodium/Argon2id/oversized control line):
		// fatal, not provider corruption — no other provider can fix it.
		dsFatalError
	};

	enum EFormat
	{
		efUnknown,
		efYenc,
		efUx,
	};

	// Keep headroom for configurable article sizes and worst-case yEnc escaping
	// (roughly twice the payload). The 16 MiB wire cap bounds staging to one
	// article: wire, restored, ciphertext and plaintext copies are each at
	// most one wire-sized buffer (<= 16 MiB), so the enforced bound is about
	// 64 MiB total. Control-line restoration adds only O(1) line descriptors
	// (lines 1-3 and the footer), never one per physical line.
	static constexpr size_t MAX_WIRE_BUFFER_SIZE = 16 * 1024 * 1024;

	Decoder();
	EStatus Check();
	void Clear();
	int DecodeBuffer(char* buffer, int len, bool alreadyUnstuffed = false);
	void SetCrcCheck(bool crcCheck) { m_crcCheck = crcCheck; }
	void SetRawMode(bool rawMode) { m_rawMode = rawMode; }
	EFormat GetFormat() { return m_format; }
	int64 GetBeginPos() { return m_beginPos; }
	int64 GetEndPos() { return m_endPos; }
	int64 GetSize() { return m_size; }
	uint32 GetExpectedCrc() { return m_expectedCRC; }
	uint32 GetCalculatedCrc() { return m_calculatedCRC; }
	bool GetEof() { return m_eof; }
	const char* GetArticleFilename() { return m_articleFilename.c_str(); }

	void SetDecryptor(YEncDecryptor* decryptor) { m_decryptor = decryptor; }
	YEncDecryptor* GetDecryptor() const { return m_decryptor; }
	bool IsWireTerminated() const { return m_wireTerminated; }
	bool IsEncrypted() const { return m_encrypted; }
	const std::vector<uint8_t>& GetDecryptedData() const { return m_decryptedPlaintext; }

#ifdef NZBGET_TESTING_HOOKS
	// Test-only surface: accessors for testing internal decoder wire state.
	// Never compiled into release builds (define NZBGET_TESTING_HOOKS in the
	// test target only).
	uint32 GetSegmentIndex() const { return m_segmentIndex; }
	int GetPartNumber() const { return m_partNumber; }
	void SetAuthFailed(bool authFailed) { m_authFailed = authFailed; }
#endif

private:
	/**
	 * @brief Extracts the begin and end positions from a "=ypart" line.
	 *
	 * Parses the "=ypart" line to find " begin=" and " end=".
	 * @param bufferEnd Exclusive end of the bounded line buffer; only
	 * meaningful together with the encrypted-path duplicate check.
	*/
	void ParseYpart(const char* buffer, const char* bufferEnd = nullptr);

	/**
	 * @brief Validates duplicate encrypted-path geometry keywords.
	 *
	 * Body Std v1.2 §2.1: if part=/size= (=ybegin) or begin=/end= (=ypart)
	 * appears more than once, every occurrence must be a well-formed
	 * decimal (entire token consumed) and all occurrences must be equal;
	 * otherwise the encrypted path fails closed (m_authFailed, no
	 * plaintext). Only tokens before any name= are inspected, mirroring
	 * the encrypted ParseName/ProcessYenc handling; ordinary yEnc never
	 * calls this. The checked-decimal grammar is shared with the
	 * encrypted part= token parser in Decoder.cpp.
	 */
	void CheckEncryptedDuplicateGeometry(const char* buffer, const char* bufferEnd);

	/**
	 * @brief Extracts the filename from a yEnc "name=" field.
	 *
	 * Parses the "name=" field, handling cases where "=ypart" is on the same line.
	 * If "=ypart" is found within the name, ParseYpart is called.
	 * @param buffer Pointer to the start of the "name=" field value (after "name=").
	 * @param bufferEnd Pointer to the end of the buffer (typically end of line).
	 * @note This function handles the unusual case where "=ypart" information is present
	 *       on the same line as the "name=" field.  This can happen if the yEnc encoder
	 *       doesn't properly separate the header lines, e.g.:
	 * 		 =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat=ypart begin=1 end=100000
	 *		 .... data
	 *		 =yend size=100000 part=1 pcrc32=abcdef12
	 * 		 Normal message:
	 *	     =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat
	 *       =ypart begin=1 end=100000
	 *		 .... data
	 *		 =yend size=100000 part=1 pcrc32=abcdef12
	*/
	void ParseName(const char* buffer, const char* bufferEnd);

	EFormat m_format = efUnknown;
	bool m_begin;
	bool m_part;
	int m_partNumber = 1;
	std::string m_rawArticleFilename;
	bool m_body;
	bool m_end;
	bool m_crc;
	uint32 m_expectedCRC;
	uint32 m_calculatedCRC;
	int64 m_beginPos;
	int64 m_endPos;
	int64 m_size;
	int64 m_endSize;
	int64 m_outSize;
	bool m_eof;
	bool m_crcCheck;
	int m_state;
	bool m_rawMode = false;
	std::string m_articleFilename;
	StringBuilder m_lineBuf;
	Crc32 m_crc32;
	YEncDecryptor* m_decryptor = nullptr;
	uint32 m_segmentIndex = 0;
	bool m_encrypted = false;
	bool m_authFailed = false;
	bool m_fatalError = false;
	uint8_t m_salt[16] = {0};
	uint8_t m_tag[16] = {0};
	std::vector<uint8_t> m_cipherPayload;
	std::vector<uint8_t> m_decryptedPlaintext;

	bool m_encryptedWireMode = false;
	bool m_wireProcessed = false;
	std::string m_wireBuffer;

	// Last raw (still dot-stuffed) wire bytes, used to find the end-of-article
	// marker before unstuffing: a stuffed data line ".." that ends a chunk
	// becomes "." after unstuffing and would look like the terminator.
	// Starts as a line break so a terminator at the very start is recognized.
	std::string m_rawTail = "\r\n";
	bool m_wireTerminated = false;

	// RFC 3977 §3.1.1 dot-unstuffing state. The connection layer
	// delivers raw socket bytes (single unstuffing owner: this decoder, before
	// any line splitting or bootstrap extraction).
	bool m_unstuffAtLineStart = true;
	bool m_unstuffPendingDot = false;

	void UnstuffInPlace(char* buffer, int& len);
	void TrackWireTerminator(const char* buffer, int len);
	void ProcessRestoredBlock(const std::string& wireBlock);
	EFormat DetectFormat(const char* buffer, int len);
	void ProcessYenc(char* buffer, int len);
	int DecodeYenc(char* buffer, char* outbuf, int len);
	EStatus CheckYenc();
	int DecodeUx(const char* inbuf, int len, char* outbuf);
	EStatus CheckUx();
	void ProcessRaw(char* buffer, int len);
};

#endif
