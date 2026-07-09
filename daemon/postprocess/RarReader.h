/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef RARREADER_H
#define RARREADER_H

#include <string>
#include "Log.h"
#include "FileSystem.h"
#include "ContentMap.h"

#ifndef DISABLE_TLS
#include "OpenSSL.h"
#endif

/* sequential parse cursor over a ContentSource, mirroring the DiskFile
 * read/seek/eof surface the rar parser was written against */
class RarSourceCursor
{
public:
	RarSourceCursor(ContentSource& source) : m_source(source) {}
	int64 Read(void* buffer, int64 size);
	bool Seek(int64 position, DiskFile::ESeekOrigin origin = DiskFile::soSet);
	int64 Position() { return m_position; }
	bool Eof() { return m_eof; }

private:
	ContentSource& m_source;
	int64 m_position = 0;
	bool m_eof = false;
};

class RarFile
{
public:
	const char* GetFilename() const { return m_filename.c_str(); }
	uint32 GetTime() { return m_time; }
	uint32 GetAttr() { return m_attr; }
	int64 GetSize() { return m_size; }
	bool GetSplitBefore() { return m_splitBefore; }
	bool GetSplitAfter() { return m_splitAfter; }
	uint32 GetMethod() { return m_method; }
	bool GetStored() { return m_stored; }
	int64 GetPackedSize() { return m_packedSize; }
	int64 GetDataOffset() { return m_dataOffset; }
	bool GetEncryptedData() { return m_encryptedData; }
private:
	std::string m_filename;
	uint32 m_time = 0;
	uint32 m_attr = 0;
	int64 m_size = 0;
	bool m_splitBefore = false;
	bool m_splitAfter = false;
	uint32 m_method = 0;
	bool m_stored = false;
	int64 m_packedSize = 0;
	int64 m_dataOffset = -1;
	bool m_encryptedData = false;
	friend class RarVolume;
};

class RarVolume
{
public:
	using FileList = std::deque<RarFile>;

	RarVolume(const char* filename) 
		: m_filename{ filename ? filename : "" }
#ifndef DISABLE_TLS
		, m_context{ nullptr, &EVP_CIPHER_CTX_free }
#endif
		{}

	bool Read();
	bool ReadFrom(ContentSource& source);

	const char* GetFilename() const { return m_filename.c_str(); }
	int GetVersion() { return m_version; }
	uint32 GetVolumeNo() { return m_volumeNo; }
	bool GetNewNaming() { return m_newNaming; }
	bool GetHasNextVolume() { return m_hasNextVolume; }
	bool GetMultiVolume() { return m_multiVolume; }
	bool GetEncrypted() { return m_encrypted; }
	void SetPassword(const char* password) { m_password = password ? password : ""; }
	FileList* GetFiles() { return &m_files; }

private:
	struct RarBlock
	{
		uint32 crc;
		uint8 type;
		uint16 flags;
		uint64 addsize;
		uint64 trailsize;
		uint64 datasize;
	};

	FileList m_files;

	std::string m_filename;
	std::string m_password;

	bool m_newNaming = false;
	bool m_hasNextVolume = false;
	bool m_multiVolume = false;
	bool m_encrypted = false;

	int m_version = 0;
	uint32 m_volumeNo = 0;
	uint8 m_decryptKey[32];
	uint8 m_decryptIV[16];
	uint8 m_decryptBuf[16];
	uint8 m_decryptPos = 16;

#ifndef DISABLE_TLS
	OpenSSL::EVPCipherCtxPtr m_context;
#endif

	int DetectRarVersion(RarSourceCursor& file);
	void LogDebugInfo();
	bool Skip(RarSourceCursor& file, RarBlock* block, int64 size);
	bool Read(RarSourceCursor& file, RarBlock* block, void* buffer, int64 size);
	bool Read16(RarSourceCursor& file, RarBlock* block, uint16* result);
	bool Read32(RarSourceCursor& file, RarBlock* block, uint32* result);
	bool ReadV(RarSourceCursor& file, RarBlock* block, uint64* result);
	bool ReadRar3Volume(RarSourceCursor& file);
	bool ReadRar5Volume(RarSourceCursor& file);
	RarBlock ReadRar3Block(RarSourceCursor& file);
	RarBlock ReadRar5Block(RarSourceCursor& file);
	bool ReadRar3File(RarSourceCursor& file, RarBlock& block, RarFile& innerFile);
	bool ReadRar5File(RarSourceCursor& file, RarBlock& block, RarFile& innerFile);
	bool DecryptRar3Prepare(const uint8 salt[8]);
	bool DecryptRar5Prepare(uint8 kdfCount, const uint8 salt[16]);
	bool DecryptInit(int keyLength);
	bool DecryptBuf(const uint8 in[16], uint8 out[16]);
	void DecryptFree();
	bool DecryptRead(RarSourceCursor& file, void* buffer, int64 size);
};

#endif
