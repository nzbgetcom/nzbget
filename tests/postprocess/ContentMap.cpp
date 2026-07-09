/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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


#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "ContentMap.h"
#include "RarReader.h"
#include "StreamCrypto.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

namespace
{

// an in-memory member: reads always succeed inside [0, Size)
class MemoryContentSource : public ContentSource
{
public:
	MemoryContentSource(std::vector<char> data) : m_data(std::move(data)) {}
	virtual int64 Size() { return (int64)m_data.size(); }
	virtual bool Read(int64 offset, void* buffer, int64 size)
	{
		if (offset < 0 || size < 0 || offset + size > (int64)m_data.size())
		{
			return false;
		}
		memcpy(buffer, m_data.data() + offset, size);
		return true;
	}
private:
	std::vector<char> m_data;
};

class MemorySourceSet : public ContentSourceSet
{
public:
	virtual ContentSource* GetSource(int memberIndex)
	{
		return memberIndex >= 0 && memberIndex < (int)Sources.size() ?
			Sources[memberIndex].get() : nullptr;
	}
	std::vector<std::unique_ptr<MemoryContentSource>> Sources;
};

std::vector<char> Pattern(int size, char seed)
{
	std::vector<char> data(size);
	for (int i = 0; i < size; i++)
	{
		data[i] = (char)(seed + i * 7);
	}
	return data;
}

void PutLe16(std::vector<char>& out, uint16 value)
{
	out.push_back((char)(value & 0xff));
	out.push_back((char)((value >> 8) & 0xff));
}

void PutLe32(std::vector<char>& out, uint32 value)
{
	for (int i = 0; i < 4; i++)
	{
		out.push_back((char)((value >> (8 * i)) & 0xff));
	}
}

void PutVint(std::vector<char>& out, uint64 value)
{
	do
	{
		uint8 sevenBits = value & 0x7f;
		value >>= 7;
		out.push_back((char)(sevenBits | (value ? 0x80 : 0)));
	} while (value);
}

// one minimal RAR3 volume: MAIN + one FILE block (store method by default,
// carrying `slice` of an inner file of fullSize bytes) + ENDARC
std::vector<char> BuildRar3StoreVolume(const std::vector<char>& slice, int64 fullSize,
	bool splitBefore, bool splitAfter, const char* innerName = "inner.mkv",
	uint8 method = 0x30)
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
	out.insert(out.end(), signature, signature + 7);

	// MAIN: VOLUME | NEWNUMBERING, 13-byte header (6 reserved bytes)
	PutLe16(out, 0);						// header crc (parser ignores it)
	out.push_back(0x73);
	PutLe16(out, 0x0011);
	PutLe16(out, 13);
	out.insert(out.end(), 6, 0);

	// FILE: LONG_BLOCK (pack size present) + split flags
	uint16 nameLen = (uint16)strlen(innerName);
	PutLe16(out, 0);
	out.push_back(0x74);
	PutLe16(out, (uint16)(0x8000 | (splitBefore ? 0x0001 : 0) | (splitAfter ? 0x0002 : 0)));
	PutLe16(out, (uint16)(32 + nameLen));	// HEAD_SIZE incl. fixed fields + name
	PutLe32(out, (uint32)slice.size());		// PACK_SIZE
	PutLe32(out, (uint32)fullSize);			// UNP_SIZE
	out.push_back(0);						// HOST_OS
	PutLe32(out, 0);						// FILE_CRC
	PutLe32(out, 0);						// FTIME
	out.push_back(29);						// UNP_VER
	out.push_back((char)method);			// METHOD (0x30 = storing)
	PutLe16(out, nameLen);
	PutLe32(out, 0x20);						// ATTR
	out.insert(out.end(), innerName, innerName + nameLen);
	out.insert(out.end(), slice.begin(), slice.end());

	// ENDARC, no optional fields
	PutLe16(out, 0);
	out.push_back(0x7b);
	PutLe16(out, 0);
	PutLe16(out, 7);

	return out;
}

// one minimal RAR5 volume: signature + MAIN + FILE (data area = slice) + ENDARC
std::vector<char> BuildRar5StoreVolume(const std::vector<char>& slice, int64 fullSize,
	bool splitBefore, bool splitAfter, const char* innerName = "inner.mkv",
	uint64 method = 0)
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00};
	out.insert(out.end(), signature, signature + 8);

	std::vector<char> mainHeader;
	PutVint(mainHeader, 1);					// type: main
	PutVint(mainHeader, 0);					// block flags
	PutVint(mainHeader, 0x01);				// arc flags: volume
	PutLe32(out, 0);						// crc (parser ignores it)
	PutVint(out, mainHeader.size());
	out.insert(out.end(), mainHeader.begin(), mainHeader.end());

	std::vector<char> fileHeader;
	PutVint(fileHeader, 2);					// type: file
	PutVint(fileHeader, 0x02 | (splitBefore ? 0x08 : 0) | (splitAfter ? 0x10 : 0));
	PutVint(fileHeader, slice.size());		// data area size
	PutVint(fileHeader, 0);					// file flags (no mtime/crc)
	PutVint(fileHeader, (uint64)fullSize);	// unpacked size
	PutVint(fileHeader, 0);					// attributes
	PutVint(fileHeader, method << 7);		// compression info (method bits 7..9)
	PutVint(fileHeader, 0);					// host os
	PutVint(fileHeader, strlen(innerName));
	fileHeader.insert(fileHeader.end(), innerName, innerName + strlen(innerName));
	PutLe32(out, 0);
	PutVint(out, fileHeader.size());
	out.insert(out.end(), fileHeader.begin(), fileHeader.end());
	out.insert(out.end(), slice.begin(), slice.end());

	std::vector<char> endHeader;
	PutVint(endHeader, 5);					// type: end of archive
	PutVint(endHeader, 0);					// block flags
	PutVint(endHeader, 0);					// end flags: no next volume
	PutLe32(out, 0);
	PutVint(out, endHeader.size());
	out.insert(out.end(), endHeader.begin(), endHeader.end());

	return out;
}

// like BuildRar3StoreVolume (no split), but the FILE block also advertises
// RAR3_FILE_PASSWORD (0x0004) so GetEncryptedData() is true while the
// volume's own headers still parse in the clear (store method)
std::vector<char> BuildRar3EncryptedDataVolume(const std::vector<char>& slice, int64 fullSize,
	const char* innerName = "inner.mkv")
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
	out.insert(out.end(), signature, signature + 7);

	PutLe16(out, 0);
	out.push_back(0x73);
	PutLe16(out, 0x0011);
	PutLe16(out, 13);
	out.insert(out.end(), 6, 0);

	uint16 nameLen = (uint16)strlen(innerName);
	PutLe16(out, 0);
	out.push_back(0x74);
	PutLe16(out, (uint16)(0x8000 | 0x0004));	// LONG_BLOCK | FILE_PASSWORD
	PutLe16(out, (uint16)(32 + nameLen));
	PutLe32(out, (uint32)slice.size());
	PutLe32(out, (uint32)fullSize);
	out.push_back(0);
	PutLe32(out, 0);
	PutLe32(out, 0);
	out.push_back(29);
	out.push_back(0x30);	// storing
	PutLe16(out, nameLen);
	PutLe32(out, 0x20);
	out.insert(out.end(), innerName, innerName + nameLen);
	out.insert(out.end(), slice.begin(), slice.end());

	PutLe16(out, 0);
	out.push_back(0x7b);
	PutLe16(out, 0);
	PutLe16(out, 7);

	return out;
}

// a bare RAR3 signature + MAIN block advertising a password: parsing aborts
// right after MAIN, leaving GetEncrypted() true and no file entries
std::vector<char> BuildRar3EncryptedHeaderVolume()
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
	out.insert(out.end(), signature, signature + 7);

	PutLe16(out, 0);
	out.push_back(0x73);
	PutLe16(out, 0x0091);	// VOLUME | NEWNUMBERING | PASSWORD
	PutLe16(out, 13);
	out.insert(out.end(), 6, 0);

	return out;
}

// a RAR3 store volume whose data is AES-encrypted: like BuildRar3StoreVolume
// but the FILE block also advertises FILE_PASSWORD (0x0004) and FILE_SALT
// (0x0400), appending the 8-byte per-file data salt after the name. `cipher` is
// the on-disk ciphertext for this volume's slice; `fullSize` is the plaintext
// inner file size. Headers stay in the clear (no MAIN password), so the parser
// exposes GetEncryptedData()/GetHasSalt()/GetSalt() without a header key.
std::vector<char> BuildRar3EncStoreVolume(const std::vector<char>& cipher, int64 fullSize,
	bool splitBefore, bool splitAfter, const uint8 salt[8], const char* innerName = "inner.mkv")
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
	out.insert(out.end(), signature, signature + 7);

	PutLe16(out, 0);
	out.push_back(0x73);
	PutLe16(out, 0x0011);					// VOLUME | NEWNUMBERING
	PutLe16(out, 13);
	out.insert(out.end(), 6, 0);

	uint16 nameLen = (uint16)strlen(innerName);
	PutLe16(out, 0);
	out.push_back(0x74);
	PutLe16(out, (uint16)(0x8000 | 0x0400 | 0x0004 |	// ADDSIZE | SALT | PASSWORD
		(splitBefore ? 0x0001 : 0) | (splitAfter ? 0x0002 : 0)));
	PutLe16(out, (uint16)(32 + nameLen + 8));	// HEAD_SIZE incl. name + 8-byte salt
	PutLe32(out, (uint32)cipher.size());		// PACK_SIZE (ciphertext bytes)
	PutLe32(out, (uint32)fullSize);				// UNP_SIZE (plaintext size)
	out.push_back(0);							// HOST_OS
	PutLe32(out, 0);							// FILE_CRC
	PutLe32(out, 0);							// FTIME
	out.push_back(29);							// UNP_VER
	out.push_back(0x30);						// METHOD store
	PutLe16(out, nameLen);
	PutLe32(out, 0x20);							// ATTR
	out.insert(out.end(), innerName, innerName + nameLen);
	out.insert(out.end(), salt, salt + 8);
	out.insert(out.end(), cipher.begin(), cipher.end());

	PutLe16(out, 0);
	out.push_back(0x7b);
	PutLe16(out, 0);
	PutLe16(out, 7);
	return out;
}

// a RAR5 store volume whose data is AES-encrypted: like BuildRar5StoreVolume
// but the FILE header carries an FHEXTRA_CRYPT record (salt/IV/kdf, and a
// password-check value when `check` is non-null). `cipher` is this volume's
// on-disk ciphertext; `fullSize` is the plaintext inner file size.
std::vector<char> BuildRar5EncStoreVolume(const std::vector<char>& cipher, int64 fullSize,
	bool splitBefore, bool splitAfter, uint8 kdfCount, const uint8 salt[16], const uint8 iv[16],
	const uint8* check, const char* innerName = "inner.mkv")
{
	std::vector<char> out;
	const char signature[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00};
	out.insert(out.end(), signature, signature + 8);

	std::vector<char> mainHeader;
	PutVint(mainHeader, 1);					// type: main
	PutVint(mainHeader, 0);					// block flags
	PutVint(mainHeader, 0x01);				// arc flags: volume
	PutLe32(out, 0);
	PutVint(out, mainHeader.size());
	out.insert(out.end(), mainHeader.begin(), mainHeader.end());

	// crypt record body (after the record-type field)
	std::vector<char> content;
	PutVint(content, 0);					// crypt version: 0 = AES-256
	PutVint(content, check ? 0x01 : 0);		// flags: bit0 = password check present
	content.push_back((char)kdfCount);
	content.insert(content.end(), salt, salt + 16);
	content.insert(content.end(), iv, iv + 16);
	if (check)
	{
		content.insert(content.end(), check, check + 12);
	}
	std::vector<char> record;
	PutVint(record, 0x01);					// record type: FHEXTRA_CRYPT
	record.insert(record.end(), content.begin(), content.end());
	std::vector<char> extra;
	PutVint(extra, record.size());
	extra.insert(extra.end(), record.begin(), record.end());

	std::vector<char> fileHeader;
	PutVint(fileHeader, 2);					// type: file
	PutVint(fileHeader, 0x01 | 0x02 |		// HAS_EXTRA | HAS_DATA
		(splitBefore ? 0x08 : 0) | (splitAfter ? 0x10 : 0));
	PutVint(fileHeader, extra.size());		// extra area size
	PutVint(fileHeader, cipher.size());		// data area size (ciphertext)
	PutVint(fileHeader, 0);					// file flags (no mtime/crc)
	PutVint(fileHeader, (uint64)fullSize);	// unpacked size (plaintext)
	PutVint(fileHeader, 0);					// attributes
	PutVint(fileHeader, 0);					// compression info: store (method 0)
	PutVint(fileHeader, 0);					// host os
	PutVint(fileHeader, strlen(innerName));
	fileHeader.insert(fileHeader.end(), innerName, innerName + strlen(innerName));
	fileHeader.insert(fileHeader.end(), extra.begin(), extra.end());
	PutLe32(out, 0);
	PutVint(out, fileHeader.size());
	out.insert(out.end(), fileHeader.begin(), fileHeader.end());
	out.insert(out.end(), cipher.begin(), cipher.end());

	std::vector<char> endHeader;
	PutVint(endHeader, 5);					// type: end of archive
	PutVint(endHeader, 0);
	PutVint(endHeader, 0);
	PutLe32(out, 0);
	PutVint(out, endHeader.size());
	out.insert(out.end(), endHeader.begin(), endHeader.end());
	return out;
}

// a minimal zip: local headers + stored data + central directory + EOCD.
// method/flags apply to every entry (8/1 model compressed/encrypted zips)
std::vector<char> BuildStoredZip(
	const std::vector<std::pair<std::string, std::vector<char>>>& files,
	uint16 method = 0, uint16 flags = 0)
{
	std::vector<char> out;
	std::vector<uint32> localOffsets;

	for (const std::pair<std::string, std::vector<char>>& file : files)
	{
		localOffsets.push_back((uint32)out.size());
		PutLe32(out, 0x04034b50);
		PutLe16(out, 20);						// version needed
		PutLe16(out, flags);
		PutLe16(out, method);
		PutLe16(out, 0);						// mod time
		PutLe16(out, 0);						// mod date
		PutLe32(out, 0);						// crc (mapper ignores it)
		PutLe32(out, (uint32)file.second.size());	// compressed size
		PutLe32(out, (uint32)file.second.size());	// uncompressed size
		PutLe16(out, (uint16)file.first.size());
		PutLe16(out, 0);						// extra length
		out.insert(out.end(), file.first.begin(), file.first.end());
		out.insert(out.end(), file.second.begin(), file.second.end());
	}

	uint32 cdStart = (uint32)out.size();
	for (size_t i = 0; i < files.size(); i++)
	{
		PutLe32(out, 0x02014b50);
		PutLe16(out, 20);						// version made by
		PutLe16(out, 20);						// version needed
		PutLe16(out, flags);
		PutLe16(out, method);
		PutLe16(out, 0);						// mod time
		PutLe16(out, 0);						// mod date
		PutLe32(out, 0);						// crc
		PutLe32(out, (uint32)files[i].second.size());
		PutLe32(out, (uint32)files[i].second.size());
		PutLe16(out, (uint16)files[i].first.size());
		PutLe16(out, 0);						// extra length
		PutLe16(out, 0);						// comment length
		PutLe16(out, 0);						// disk number start
		PutLe16(out, 0);						// internal attributes
		PutLe32(out, 0);						// external attributes
		PutLe32(out, localOffsets[i]);
		out.insert(out.end(), files[i].first.begin(), files[i].first.end());
	}
	uint32 cdSize = (uint32)out.size() - cdStart;

	PutLe32(out, 0x06054b50);
	PutLe16(out, 0);							// this disk
	PutLe16(out, 0);							// cd start disk
	PutLe16(out, (uint16)files.size());			// entries this disk
	PutLe16(out, (uint16)files.size());			// entries total
	PutLe32(out, cdSize);
	PutLe32(out, cdStart);
	PutLe16(out, 0);							// comment length
	return out;
}

void Put7zNumber(std::vector<char>& out, uint64 value)
{
	uint8 firstByte = 0;
	uint8 mask = 0x80;
	int extraBytes = 0;
	while (extraBytes < 8 && value >= ((uint64)1 << (7 * (extraBytes + 1))))
	{
		firstByte |= mask;
		mask >>= 1;
		extraBytes++;
	}
	if (extraBytes < 8)
	{
		firstByte |= (uint8)(value >> (8 * extraBytes));
	}
	out.push_back((char)firstByte);
	for (int i = 0; i < extraBytes; i++)
	{
		out.push_back((char)((value >> (8 * i)) & 0xff));
	}
}

// a minimal 7z archive, Copy coder, one folder per file, plain kHeader
std::vector<char> Build7zCopy(
	const std::vector<std::pair<std::string, std::vector<char>>>& files,
	uint8 coderId = 0x00)
{
	std::vector<char> header;
	header.push_back(0x01);							// kHeader
	header.push_back(0x04);							// kMainStreamsInfo
	header.push_back(0x06);							// kPackInfo
	Put7zNumber(header, 0);							// pack position
	Put7zNumber(header, files.size());				// pack stream count
	header.push_back(0x09);							// kSize
	for (const auto& file : files)
	{
		Put7zNumber(header, file.second.size());
	}
	header.push_back(0x00);							// kEnd (pack info)
	header.push_back(0x07);							// kUnPackInfo
	header.push_back(0x0b);							// kFolder
	Put7zNumber(header, files.size());				// folder count
	header.push_back(0x00);							// external
	for (size_t i = 0; i < files.size(); i++)
	{
		Put7zNumber(header, 1);						// one coder
		header.push_back(0x01);						// flags: id size 1
		header.push_back((char)coderId);			// 0x00 = Copy
	}
	header.push_back(0x0c);							// kCodersUnpackSize
	for (const auto& file : files)
	{
		Put7zNumber(header, file.second.size());
	}
	header.push_back(0x00);							// kEnd (unpack info)
	header.push_back(0x00);							// kEnd (streams info)
	header.push_back(0x05);							// kFilesInfo
	Put7zNumber(header, files.size());
	std::vector<char> names;
	names.push_back(0x00);							// external
	for (const auto& file : files)
	{
		for (char ch : file.first)
		{
			names.push_back(ch);
			names.push_back(0x00);
		}
		names.push_back(0x00);
		names.push_back(0x00);
	}
	header.push_back(0x11);							// kName
	Put7zNumber(header, names.size());
	header.insert(header.end(), names.begin(), names.end());
	header.push_back(0x00);							// end of file properties
	header.push_back(0x00);							// kEnd (header)

	std::vector<char> out;
	const char signature[] = {'7', 'z', (char)0xbc, (char)0xaf, 0x27, 0x1c};
	out.insert(out.end(), signature, signature + 6);
	out.push_back(0);								// version major
	out.push_back(4);								// version minor
	PutLe32(out, 0);								// start header crc (unchecked)
	int64 dataSize = 0;
	for (const auto& file : files)
	{
		dataSize += (int64)file.second.size();
	}
	for (int i = 0; i < 8; i++)						// next header offset
	{
		out.push_back((char)(((uint64)dataSize >> (8 * i)) & 0xff));
	}
	for (int i = 0; i < 8; i++)						// next header size
	{
		out.push_back((char)(((uint64)header.size() >> (8 * i)) & 0xff));
	}
	PutLe32(out, 0);								// next header crc (unchecked)
	for (const auto& file : files)
	{
		out.insert(out.end(), file.second.begin(), file.second.end());
	}
	out.insert(out.end(), header.begin(), header.end());
	return out;
}

// signature header + pack data + raw header bytes (for adversarial headers)
std::vector<char> Wrap7zHeader(const std::vector<char>& header,
	const std::vector<char>& data)
{
	std::vector<char> out;
	const char signature[] = {'7', 'z', (char)0xbc, (char)0xaf, 0x27, 0x1c};
	out.insert(out.end(), signature, signature + 6);
	out.push_back(0);								// version major
	out.push_back(4);								// version minor
	PutLe32(out, 0);								// start header crc (unchecked)
	for (int i = 0; i < 8; i++)						// next header offset
	{
		out.push_back((char)(((uint64)data.size() >> (8 * i)) & 0xff));
	}
	for (int i = 0; i < 8; i++)						// next header size
	{
		out.push_back((char)(((uint64)header.size() >> (8 * i)) & 0xff));
	}
	PutLe32(out, 0);								// next header crc (unchecked)
	out.insert(out.end(), data.begin(), data.end());
	out.insert(out.end(), header.begin(), header.end());
	return out;
}

// a kFilesInfo record naming one file (for adversarial headers)
void Append7zSingleFileInfo(std::vector<char>& header, const char* name)
{
	header.push_back(0x05);							// kFilesInfo
	Put7zNumber(header, 1);
	std::vector<char> names;
	names.push_back(0x00);							// external
	for (const char* ch = name; *ch; ch++)
	{
		names.push_back(*ch);
		names.push_back(0x00);
	}
	names.push_back(0x00);
	names.push_back(0x00);
	header.push_back(0x11);							// kName
	Put7zNumber(header, names.size());
	header.insert(header.end(), names.begin(), names.end());
	header.push_back(0x00);							// end of file properties
}

}

BOOST_AUTO_TEST_CASE(ContentMapModelTest)
{
	// inner file of 100 bytes inside two members, 20 bytes of framing before
	// each data run: member 0 carries inner [0,60), member 1 carries [60,100)
	ContentMap map;
	map.SetInnerName("movie.mkv");
	map.SetInnerSize(100);
	map.GetRuns()->push_back({0, 0, 20, 60});
	map.GetRuns()->push_back({60, 1, 20, 40});

	// member range straddling framing and data: only the data part maps
	StreamRangeList inner = map.MapToInner(0, {0, 30});
	BOOST_REQUIRE_EQUAL(inner.size(), 1u);
	BOOST_CHECK_EQUAL(inner[0].Offset, 0);
	BOOST_CHECK_EQUAL(inner[0].Size, 10);

	// inner range straddling the member boundary splits into two pieces
	std::vector<MemberRange> pieces = map.MapFromInner({50, 20});
	BOOST_REQUIRE_EQUAL(pieces.size(), 2u);
	BOOST_CHECK_EQUAL(pieces[0].MemberIndex, 0);
	BOOST_CHECK_EQUAL(pieces[0].Range.Offset, 70);	// 20 framing + (50-0)
	BOOST_CHECK_EQUAL(pieces[0].Range.Size, 10);
	BOOST_CHECK_EQUAL(pieces[1].MemberIndex, 1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Offset, 20);
	BOOST_CHECK_EQUAL(pieces[1].Range.Size, 10);

	// a hole entirely inside framing maps to nothing
	BOOST_CHECK(map.MapToInner(0, {0, 20}).empty());

	// excluding a member drops its runs in both directions
	map.ExcludeMember(0);
	BOOST_CHECK(map.MapToInner(0, {20, 60}).empty());
	std::vector<MemberRange> after = map.MapFromInner({0, 100});
	BOOST_REQUIRE_EQUAL(after.size(), 1u);
	BOOST_CHECK_EQUAL(after[0].MemberIndex, 1);
}

BOOST_AUTO_TEST_CASE(CompositeSourceTest)
{
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(10, 1)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(20, 2)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(5, 3)));

	CompositeSource composite(sources, {0, 1, 2}, {10, 20, 5});
	BOOST_CHECK_EQUAL(composite.Size(), 35);

	// a read across the 0/1 boundary stitches both members
	char buffer[15];
	BOOST_REQUIRE(composite.Read(5, buffer, 15));
	std::vector<char> member0 = Pattern(10, 1), member1 = Pattern(20, 2);
	BOOST_CHECK(!memcmp(buffer, member0.data() + 5, 5));
	BOOST_CHECK(!memcmp(buffer + 5, member1.data(), 10));

	// past-end reads fail all-or-nothing
	BOOST_CHECK(!composite.Read(30, buffer, 10));

	// logical->member translation splits across boundaries
	std::vector<MemberRange> pieces = composite.ToMembers({8, 24});
	BOOST_REQUIRE_EQUAL(pieces.size(), 3u);
	BOOST_CHECK_EQUAL(pieces[0].MemberIndex, 0);
	BOOST_CHECK_EQUAL(pieces[0].Range.Offset, 8);
	BOOST_CHECK_EQUAL(pieces[0].Range.Size, 2);
	BOOST_CHECK_EQUAL(pieces[1].MemberIndex, 1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Offset, 0);
	BOOST_CHECK_EQUAL(pieces[1].Range.Size, 20);
	BOOST_CHECK_EQUAL(pieces[2].MemberIndex, 2);
	BOOST_CHECK_EQUAL(pieces[2].Range.Offset, 0);
	BOOST_CHECK_EQUAL(pieces[2].Range.Size, 2);
}

BOOST_AUTO_TEST_CASE(ContentMapperGroupSetsTest)
{
	std::vector<SetMember> members = {
		{"Rel.part02.rar", 100}, {"Rel.part01.rar", 100},		// 0,1: new naming, out of order
		{"old.r00", 100}, {"old.rar", 100}, {"old.r01", 100},	// 2,3,4: old naming
		{"span.z01", 100}, {"span.zip", 100}, {"span.z02", 100},	// 5,6,7: spanned zip
		{"seven.7z.002", 100}, {"seven.7z.001", 100},			// 8,9: 7z splits
		{"movie.mkv.001", 100}, {"movie.mkv.002", 100},			// 10,11: raw splits
		{"bare.mkv", 100},										// 12: bare media
		{"Rel.vol00+01.par2", 100},								// 13: no set
		{"gap.part01.rar", 100}, {"gap.part03.rar", 100},		// 14,15: incomplete -> dropped
		{"readme.txt", 100},									// 16: no set
	};

	std::vector<MemberSet> sets = ContentMapper::GroupSets(members);
	BOOST_REQUIRE_EQUAL(sets.size(), 6u);

	// new-naming rar, ordered by part number regardless of listing order
	BOOST_CHECK_EQUAL((int)sets[0].Format, (int)MemberSet::mfRar);
	BOOST_REQUIRE_EQUAL(sets[0].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[0].Members[0], 1);
	BOOST_CHECK_EQUAL(sets[0].Members[1], 0);

	// old-naming rar: .rar first, then .r00, .r01
	BOOST_CHECK_EQUAL((int)sets[1].Format, (int)MemberSet::mfRar);
	BOOST_REQUIRE_EQUAL(sets[1].Members.size(), 3u);
	BOOST_CHECK_EQUAL(sets[1].Members[0], 3);
	BOOST_CHECK_EQUAL(sets[1].Members[1], 2);
	BOOST_CHECK_EQUAL(sets[1].Members[2], 4);

	// spanned zip: z01, z02, then .zip LAST (data order)
	BOOST_CHECK_EQUAL((int)sets[2].Format, (int)MemberSet::mfZip);
	BOOST_REQUIRE_EQUAL(sets[2].Members.size(), 3u);
	BOOST_CHECK_EQUAL(sets[2].Members[0], 5);
	BOOST_CHECK_EQUAL(sets[2].Members[1], 7);
	BOOST_CHECK_EQUAL(sets[2].Members[2], 6);

	// 7z splits ordered by suffix number
	BOOST_CHECK_EQUAL((int)sets[3].Format, (int)MemberSet::mfSevenZip);
	BOOST_REQUIRE_EQUAL(sets[3].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[3].Members[0], 9);
	BOOST_CHECK_EQUAL(sets[3].Members[1], 8);

	// raw splits of a media file
	BOOST_CHECK_EQUAL((int)sets[4].Format, (int)MemberSet::mfSplit);
	BOOST_REQUIRE_EQUAL(sets[4].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[4].Members[0], 10);
	BOOST_CHECK_EQUAL(sets[4].Members[1], 11);

	// bare media singleton; par2/txt and the gapped rar set are absent
	BOOST_CHECK_EQUAL((int)sets[5].Format, (int)MemberSet::mfBare);
	BOOST_REQUIRE_EQUAL(sets[5].Members.size(), 1u);
	BOOST_CHECK_EQUAL(sets[5].Members[0], 12);
}

BOOST_AUTO_TEST_CASE(ContentMapperSplitAndBareMapTest)
{
	std::vector<SetMember> members = {
		{"movie.mkv.001", 0}, {"movie.mkv.002", 0}, {"bare.mkv", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(30, 1)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(12, 2)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(50, 3)));

	std::string skipReason;

	// raw splits concatenate: inner name loses the numeric suffix
	MemberSet splitSet{MemberSet::mfSplit, {0, 1}};
	std::unique_ptr<ContentMap> splitMap =
		ContentMapper::BuildMap(members, splitSet, sources, skipReason);
	BOOST_REQUIRE(splitMap);
	BOOST_CHECK_EQUAL(splitMap->GetInnerName(), "movie.mkv");
	BOOST_CHECK_EQUAL(splitMap->GetInnerSize(), 42);
	BOOST_REQUIRE_EQUAL(splitMap->GetRuns()->size(), 2u);
	BOOST_CHECK_EQUAL((*splitMap->GetRuns())[1].InnerOffset, 30);
	BOOST_CHECK_EQUAL((*splitMap->GetRuns())[1].MemberOffset, 0);

	// bare media is the identity map
	MemberSet bareSet{MemberSet::mfBare, {2}};
	std::unique_ptr<ContentMap> bareMap =
		ContentMapper::BuildMap(members, bareSet, sources, skipReason);
	BOOST_REQUIRE(bareMap);
	BOOST_CHECK_EQUAL(bareMap->GetInnerName(), "bare.mkv");
	BOOST_CHECK_EQUAL(bareMap->GetInnerSize(), 50);
	BOOST_REQUIRE_EQUAL(bareMap->GetRuns()->size(), 1u);
	BOOST_CHECK_EQUAL((*bareMap->GetRuns())[0].MemberOffset, 0);

	// an unservable member kills the split map with a reason
	MemorySourceSet holed;
	holed.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(30, 1)));
	// member 1 missing entirely
	MemberSet badSet{MemberSet::mfSplit, {0, 1}};
	BOOST_CHECK(!ContentMapper::BuildMap(members, badSet, holed, skipReason));
	BOOST_CHECK(!skipReason.empty());

	// rar mapping is not implemented until Task 4 - and once it is, these
	// non-rar bytes still fail with a reason, so this assertion outlives it
	MemberSet rarSet{MemberSet::mfRar, {0}};
	BOOST_CHECK(!ContentMapper::BuildMap(members, rarSet, sources, skipReason));
	BOOST_CHECK(!skipReason.empty());
}

BOOST_AUTO_TEST_CASE(ContentMapperRarStoreMapTest)
{
	std::vector<char> inner = Pattern(100, 5);
	std::vector<char> slice0(inner.begin(), inner.begin() + 40);
	std::vector<char> slice1(inner.begin() + 40, inner.begin() + 80);
	std::vector<char> slice2(inner.begin() + 80, inner.end());

	std::vector<SetMember> members = {
		{"rel.part01.rar", 0}, {"rel.part02.rar", 0}, {"rel.part03.rar", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3StoreVolume(slice0, 100, false, true)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3StoreVolume(slice1, 100, true, true)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3StoreVolume(slice2, 100, true, false)));

	MemberSet set{MemberSet::mfRar, {0, 1, 2}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason);
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK_EQUAL(map->GetInnerName(), "inner.mkv");
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 100);
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 3u);

	// reading the whole inner stream through the map reproduces the payload
	std::vector<char> reassembled(100);
	for (const MemberRange& piece : map->MapFromInner({0, 100}))
	{
		ContentSource* source = sources.GetSource(piece.MemberIndex);
		StreamRangeList innerPos = map->MapToInner(piece.MemberIndex, piece.Range);
		BOOST_REQUIRE_EQUAL(innerPos.size(), 1u);
		BOOST_REQUIRE(source->Read(piece.Range.Offset,
			reassembled.data() + innerPos[0].Offset, piece.Range.Size));
	}
	BOOST_CHECK(!memcmp(reassembled.data(), inner.data(), 100));

	// same content in RAR5 framing maps to the same inner stream
	MemorySourceSet sources5;
	sources5.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar5StoreVolume(slice0, 100, false, true)));
	sources5.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar5StoreVolume(slice1, 100, true, true)));
	sources5.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar5StoreVolume(slice2, 100, true, false)));
	std::unique_ptr<ContentMap> map5 =
		ContentMapper::BuildMap(members, set, sources5, skipReason);
	BOOST_REQUIRE_MESSAGE(map5, skipReason);
	BOOST_CHECK_EQUAL(map5->GetInnerSize(), 100);
	BOOST_REQUIRE_EQUAL(map5->GetRuns()->size(), 3u);
}

BOOST_AUTO_TEST_CASE(ContentMapperRarMapDegradationTest)
{
	std::vector<char> inner = Pattern(100, 6);
	std::vector<char> slice0(inner.begin(), inner.begin() + 40);
	std::vector<char> slice1(inner.begin() + 40, inner.begin() + 80);
	std::vector<char> slice2(inner.begin() + 80, inner.end());
	std::vector<SetMember> members = {
		{"rel.part01.rar", 0}, {"rel.part02.rar", 0}, {"rel.part03.rar", 0}};
	MemberSet set{MemberSet::mfRar, {0, 1, 2}};
	std::string skipReason;

	// one unreadable volume: packed size inferred, runs excluded, rest maps
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice0, 100, false, true)));
		sources.Sources.push_back(nullptr);
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice2, 100, true, false)));
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 2u);
		BOOST_CHECK(map->MapFromInner({40, 40}).empty());	// vol 2's inner region
		BOOST_CHECK_EQUAL((*map->GetRuns())[1].InnerOffset, 80);
	}

	// two unreadable volumes: the set degrades entirely
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice0, 100, false, true)));
		sources.Sources.push_back(nullptr);
		sources.Sources.push_back(nullptr);
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
	}

	// compressed method: skipped with a store-mode reason
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice0, 100, false, false, "inner.mkv", 0x33)));
		MemberSet single{MemberSet::mfRar, {0}};
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason));
		BOOST_CHECK(skipReason.find("store") != std::string::npos);
	}

	// a non-media inner file never maps
	{
		std::vector<char> whole = Pattern(40, 7);
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(whole, 40, false, false, "inner.iso")));
		MemberSet single{MemberSet::mfRar, {0}};
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "inner file is not a media file");
	}

	// packed sizes that do not sum to the inner size (a lying store set)
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice0, 100, false, true)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice1, 100, true, true)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(std::vector<char>(slice2.begin(), slice2.end() - 5),
				100, true, false)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK(skipReason.find("sum") != std::string::npos);
	}

	// packed sizes that already exceed the inner size even with one volume
	// carried as unknown: the inferred remainder would be negative
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(Pattern(60, 10), 100, false, true)));
		sources.Sources.push_back(nullptr);
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(Pattern(60, 11), 100, true, false)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK(skipReason.find("exceed") != std::string::npos);
	}

	// encrypted archive headers: a password-protected MAIN block, no
	// password supplied - M2 has no password handling (M3's territory)
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncryptedHeaderVolume()));
		MemberSet single{MemberSet::mfRar, {0}};
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "encrypted archive headers");
	}

	// encrypted archive data: headers parse fine, but the FILE block carries
	// the password flag on the data itself
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncryptedDataVolume(Pattern(100, 12), 100)));
		MemberSet single{MemberSet::mfRar, {0}};
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "encrypted archive data");
	}

	// inconsistent inner file size: the second volume's entry for the same
	// inner name reports a different unpacked size
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(Pattern(100, 13), 100, false, false)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(Pattern(30, 14), 90, false, false)));
		MemberSet pair{MemberSet::mfRar, {0, 1}};
		BOOST_CHECK(!ContentMapper::BuildMap(members, pair, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "inconsistent inner file size across volumes");
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperZipStoreMapTest)
{
	std::vector<char> inner = Pattern(90, 8);
	std::vector<char> nfo = Pattern(10, 9);
	std::vector<char> zipBytes = BuildStoredZip(
		{{"release/info.nfo", nfo}, {"release/movie.mkv", inner}});

	// a lone .zip: the largest entry becomes the inner file, nfo unmapped
	{
		std::vector<SetMember> members = {{"rel.zip", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(zipBytes));
		MemberSet set{MemberSet::mfZip, {0}};
		std::string skipReason;
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerName(), "movie.mkv");
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 90);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);

		std::vector<char> data(90);
		const ContentRun& run = (*map->GetRuns())[0];
		BOOST_REQUIRE(sources.GetSource(0)->Read(run.MemberOffset, data.data(), 90));
		BOOST_CHECK(!memcmp(data.data(), inner.data(), 90));
	}

	// the same bytes split into spanned volumes map through the composition
	{
		std::vector<SetMember> members = {{"rel.z01", 0}, {"rel.z02", 0}, {"rel.zip", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			std::vector<char>(zipBytes.begin(), zipBytes.begin() + 40)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			std::vector<char>(zipBytes.begin() + 40, zipBytes.begin() + 95)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			std::vector<char>(zipBytes.begin() + 95, zipBytes.end())));
		MemberSet set{MemberSet::mfZip, {0, 1, 2}};
		std::string skipReason;
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 90);

		// stitch the inner stream back through the runs
		std::vector<char> reassembled(90);
		for (const MemberRange& piece : map->MapFromInner({0, 90}))
		{
			StreamRangeList innerPos = map->MapToInner(piece.MemberIndex, piece.Range);
			BOOST_REQUIRE_EQUAL(innerPos.size(), 1u);
			BOOST_REQUIRE(sources.GetSource(piece.MemberIndex)->Read(piece.Range.Offset,
				reassembled.data() + innerPos[0].Offset, piece.Range.Size));
		}
		BOOST_CHECK(!memcmp(reassembled.data(), inner.data(), 90));
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperZipDegradationTest)
{
	std::vector<char> inner = Pattern(50, 10);
	std::vector<SetMember> members = {{"rel.zip", 0}};
	MemberSet set{MemberSet::mfZip, {0}};
	std::string skipReason;

	// compressed entries never map
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildStoredZip({{"movie.mkv", inner}}, 8)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK(skipReason.find("stored") != std::string::npos);
	}

	// encrypted entries never map
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildStoredZip({{"movie.mkv", inner}}, 0, 1)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK(skipReason.find("encrypted") != std::string::npos);
	}

	// zip64 per-entry extra: maxed 32-bit markers resolve through field 0x0001
	{
		std::vector<char> zipBytes = BuildStoredZip({{"movie.mkv", inner}});
		// rewrite the central entry: max out usize/csize/local offset and
		// append the zip64 extra carrying the real values
		// (locate the CD via the EOCD trailer we just wrote)
		size_t eocd = zipBytes.size() - 22;
		uint32 cdStart = (uint32)((uint8)zipBytes[eocd + 16] |
			((uint8)zipBytes[eocd + 17] << 8) | ((uint8)zipBytes[eocd + 18] << 16) |
			((uint8)zipBytes[eocd + 19] << 24));
		std::vector<char> rebuilt(zipBytes.begin(), zipBytes.begin() + cdStart);
		std::vector<char> entry(zipBytes.begin() + cdStart, zipBytes.begin() + eocd);
		// maxed markers
		for (int i = 20; i < 28; i++) entry[i] = (char)0xff;	// csize + usize
		for (int i = 42; i < 46; i++) entry[i] = (char)0xff;	// local offset
		// extra: id 0x0001, size 24: usize, csize, local offset (8 bytes each)
		entry[30] = 28; entry[31] = 0;	// extra length
		std::vector<char> extra;
		PutLe16(extra, 0x0001);
		PutLe16(extra, 24);
		for (int i = 0; i < 2; i++)
		{
			extra.push_back(50); extra.insert(extra.end(), 7, 0);	// usize, csize = 50
		}
		extra.insert(extra.end(), 8, 0);	// local offset = 0
		entry.insert(entry.end(), extra.begin(), extra.end());
		rebuilt.insert(rebuilt.end(), entry.begin(), entry.end());
		size_t newCdSize = entry.size();
		std::vector<char> trailer(zipBytes.begin() + eocd, zipBytes.end());
		// patch cd size in the EOCD copy
		trailer[12] = (char)(newCdSize & 0xff);
		trailer[13] = (char)((newCdSize >> 8) & 0xff);
		rebuilt.insert(rebuilt.end(), trailer.begin(), trailer.end());

		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(rebuilt));
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 50);
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipCopyMapTest)
{
	std::vector<char> inner = Pattern(90, 11);
	std::vector<char> nfo = Pattern(10, 12);
	std::vector<char> archive = Build7zCopy({{"info.nfo", nfo}, {"movie.mkv", inner}});

	// lone .7z
	{
		std::vector<SetMember> members = {{"rel.7z", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(archive));
		MemberSet set{MemberSet::mfSevenZip, {0}};
		std::string skipReason;
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerName(), "movie.mkv");
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 90);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);
		// the nfo pack stream sits first: movie data begins after it
		BOOST_CHECK_EQUAL((*map->GetRuns())[0].MemberOffset, 32 + 10);
	}

	// .7z.001/.7z.002 splits: same mapping through the composition
	{
		std::vector<SetMember> members = {{"rel.7z.001", 0}, {"rel.7z.002", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			std::vector<char>(archive.begin(), archive.begin() + 60)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			std::vector<char>(archive.begin() + 60, archive.end())));
		MemberSet set{MemberSet::mfSevenZip, {0, 1}};
		std::string skipReason;
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 90);

		std::vector<char> reassembled(90);
		for (const MemberRange& piece : map->MapFromInner({0, 90}))
		{
			StreamRangeList innerPos = map->MapToInner(piece.MemberIndex, piece.Range);
			BOOST_REQUIRE_EQUAL(innerPos.size(), 1u);
			BOOST_REQUIRE(sources.GetSource(piece.MemberIndex)->Read(piece.Range.Offset,
				reassembled.data() + innerPos[0].Offset, piece.Range.Size));
		}
		BOOST_CHECK(!memcmp(reassembled.data(), inner.data(), 90));
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipDegradationTest)
{
	std::vector<char> inner = Pattern(50, 13);
	std::vector<SetMember> members = {{"rel.7z", 0}};
	MemberSet set{MemberSet::mfSevenZip, {0}};
	std::string skipReason;

	// a non-Copy coder (0x21 = LZMA2) never maps
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			Build7zCopy({{"movie.mkv", inner}}, 0x21)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK(skipReason.find("opy") != std::string::npos);	// "...Copy/copy..."
	}

	// not a 7z at all
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(64, 14)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
	}

	// a non-media inner file never maps
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			Build7zCopy({{"backup.iso", inner}})));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "inner file is not a media file");
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipEncodedHeaderTest)
{
	// a kEncodedHeader whose own folder is Copy-coded: the real header bytes
	// sit verbatim in the pack area and the mapper must follow them there
	std::vector<char> inner = Pattern(30, 15);
	std::vector<char> plain = Build7zCopy({{"movie.mkv", inner}});
	std::vector<char> realHeader(plain.begin() + 32 + 30, plain.end());

	auto buildEncoded = [&](uint8 headerCoderId)
	{
		std::vector<char> top;
		top.push_back(0x17);						// kEncodedHeader
		top.push_back(0x06);						// kPackInfo
		Put7zNumber(top, 30);						// pack pos: after the file data
		Put7zNumber(top, 1);						// one pack stream
		top.push_back(0x09);						// kSize
		Put7zNumber(top, realHeader.size());
		top.push_back(0x00);						// kEnd (pack info)
		top.push_back(0x07);						// kUnPackInfo
		top.push_back(0x0b);						// kFolder
		Put7zNumber(top, 1);						// folder count
		top.push_back(0x00);						// external
		Put7zNumber(top, 1);						// one coder
		top.push_back(0x01);						// flags: id size 1
		top.push_back((char)headerCoderId);			// 0x00 = Copy
		top.push_back(0x0c);						// kCodersUnpackSize
		Put7zNumber(top, realHeader.size());
		top.push_back(0x00);						// kEnd (unpack info)
		top.push_back(0x00);						// kEnd (streams info)

		std::vector<char> out;
		const char signature[] = {'7', 'z', (char)0xbc, (char)0xaf, 0x27, 0x1c};
		out.insert(out.end(), signature, signature + 6);
		out.push_back(0);							// version major
		out.push_back(4);							// version minor
		PutLe32(out, 0);							// start header crc (unchecked)
		uint64 nextHeaderOffset = 30 + realHeader.size();
		for (int i = 0; i < 8; i++)					// next header offset
		{
			out.push_back((char)((nextHeaderOffset >> (8 * i)) & 0xff));
		}
		for (int i = 0; i < 8; i++)					// next header size
		{
			out.push_back((char)(((uint64)top.size() >> (8 * i)) & 0xff));
		}
		PutLe32(out, 0);							// next header crc (unchecked)
		out.insert(out.end(), inner.begin(), inner.end());
		out.insert(out.end(), realHeader.begin(), realHeader.end());
		out.insert(out.end(), top.begin(), top.end());
		return out;
	};

	std::vector<SetMember> members = {{"rel.7z", 0}};
	MemberSet set{MemberSet::mfSevenZip, {0}};
	std::string skipReason;

	// Copy-coded packed header: followed to the real file table
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(buildEncoded(0x00)));
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason);
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_CHECK_EQUAL(map->GetInnerName(), "movie.mkv");
		BOOST_CHECK_EQUAL(map->GetInnerSize(), 30);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);
		BOOST_CHECK_EQUAL((*map->GetRuns())[0].MemberOffset, 32);
	}

	// an LZMA2-coded (actually compressed) header never maps
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(buildEncoded(0x21)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "compressed 7z header");
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipAdversarialTest)
{
	// lying counts, sizes and offsets must degrade to a skip - never crash,
	// over-read or allocate absurd amounts (the reader fails closed)
	std::vector<SetMember> members = {{"rel.7z", 0}};
	MemberSet set{MemberSet::mfSevenZip, {0}};
	std::string skipReason;

	auto rejects = [&](const std::vector<char>& header, const std::vector<char>& data)
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			Wrap7zHeader(header, data)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
	};

	// a kArchiveProperties record claiming a 2^63 byte payload: the skip
	// must not wrap the read position out of the buffer
	{
		std::vector<char> header = {0x01, 0x02, 0x03};
		Put7zNumber(header, (uint64)1 << 63);
		rejects(header, {});
	}

	// a pack-stream count of 2^60 sizing the kCRC defined-bit vector
	{
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, (uint64)1 << 60);		// pack stream count
		header.push_back(0x0a);						// kCRC
		header.push_back(0x00);						// allAreDefined = 0: bit vector
		rejects(header, {});
	}

	// a Copy-coded packed header whose single pack stream claims 2^60 bytes
	{
		std::vector<char> header = {0x17, 0x06};	// kEncodedHeader, kPackInfo
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, 1);						// one pack stream
		header.push_back(0x09);						// kSize
		Put7zNumber(header, (uint64)1 << 60);
		header.push_back(0x00);						// kEnd (pack info)
		header.push_back(0x07);						// kUnPackInfo
		header.push_back(0x0b);						// kFolder
		Put7zNumber(header, 1);
		header.push_back(0x00);						// external
		Put7zNumber(header, 1);						// one coder
		header.push_back(0x01);						// flags: id size 1
		header.push_back(0x00);						// Copy
		header.push_back(0x0c);						// kCodersUnpackSize
		Put7zNumber(header, (uint64)1 << 60);
		header.push_back(0x00);						// kEnd (unpack info)
		header.push_back(0x00);						// kEnd (streams info)
		rejects(header, {});
	}

	// a pack position near 2^63: offset arithmetic must not wrap into a
	// bogus (empty-runs or negative) mapping
	{
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, ((uint64)1 << 63) - 16);	// pack pos
		Put7zNumber(header, 1);
		header.push_back(0x09);						// kSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (pack info)
		header.push_back(0x07);						// kUnPackInfo
		header.push_back(0x0b);						// kFolder
		Put7zNumber(header, 1);
		header.push_back(0x00);						// external
		Put7zNumber(header, 1);						// one coder
		header.push_back(0x01);						// flags: id size 1
		header.push_back(0x00);						// Copy
		header.push_back(0x0c);						// kCodersUnpackSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (unpack info)
		header.push_back(0x00);						// kEnd (streams info)
		Append7zSingleFileInfo(header, "movie.mkv");
		header.push_back(0x00);						// kEnd (header)
		rejects(header, Pattern(10, 17));
		BOOST_CHECK_EQUAL(skipReason, "implausible data run geometry");
	}

	// a folder with zero coders is spec-invalid and must not map as Copy
	{
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, 1);
		header.push_back(0x09);						// kSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (pack info)
		header.push_back(0x07);						// kUnPackInfo
		header.push_back(0x0b);						// kFolder
		Put7zNumber(header, 1);
		header.push_back(0x00);						// external
		Put7zNumber(header, 0);						// ZERO coders
		header.push_back(0x0c);						// kCodersUnpackSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (unpack info)
		header.push_back(0x00);						// kEnd (streams info)
		Append7zSingleFileInfo(header, "movie.mkv");
		header.push_back(0x00);						// kEnd (header)
		rejects(header, Pattern(10, 19));
	}

	// a pack-stream count of 2^60 driving the kSize number list
	{
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, (uint64)1 << 60);		// pack stream count
		header.push_back(0x09);						// kSize: 2^60 numbers claimed
		rejects(header, {});
	}

	// a kNumUnpackStream of 2^40 driving the per-substream kSize list
	{
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, 1);						// one pack stream
		header.push_back(0x09);						// kSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (pack info)
		header.push_back(0x07);						// kUnPackInfo
		header.push_back(0x0b);						// kFolder
		Put7zNumber(header, 1);
		header.push_back(0x00);						// external
		Put7zNumber(header, 1);						// one coder
		header.push_back(0x01);						// flags: id size 1
		header.push_back(0x00);						// Copy
		header.push_back(0x0c);						// kCodersUnpackSize
		Put7zNumber(header, 10);
		header.push_back(0x00);						// kEnd (unpack info)
		header.push_back(0x08);						// kSubStreamsInfo
		header.push_back(0x0d);						// kNumUnpackStream
		Put7zNumber(header, (uint64)1 << 40);		// 2^40 substreams claimed
		header.push_back(0x09);						// kSize
		rejects(header, Pattern(10, 16));
	}

	// a kCodersUnpackSize list truncated right after its marker: the loop
	// must stop on the poisoned reader instead of appending one dry zero
	// for every announced folder
	{
		const int folderCount = 4096;
		std::vector<char> header = {0x01, 0x04, 0x06};
		Put7zNumber(header, 0);						// pack pos
		Put7zNumber(header, folderCount);			// pack stream count
		header.push_back(0x09);						// kSize
		for (int i = 0; i < folderCount; i++)
		{
			Put7zNumber(header, 1);
		}
		header.push_back(0x00);						// kEnd (pack info)
		header.push_back(0x07);						// kUnPackInfo
		header.push_back(0x0b);						// kFolder
		Put7zNumber(header, folderCount);
		header.push_back(0x00);						// external
		for (int i = 0; i < folderCount; i++)
		{
			Put7zNumber(header, 1);					// one coder
			header.push_back(0x01);					// flags: id size 1
			header.push_back(0x00);					// Copy
		}
		header.push_back(0x0c);						// kCodersUnpackSize, then EOF
		rejects(header, {});
	}

	// a header truncated mid-structure skips, never over-reads
	{
		std::vector<char> archive = Build7zCopy({{"movie.mkv", Pattern(20, 18)}});
		archive.resize(archive.size() - 5);
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(archive));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason));
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipSubstreamsTest)
{
	// one Copy folder carrying two files as substreams (solid-style), plus an
	// empty-stream directory entry that owns no data: names must align to
	// substreams folder-major and offsets accumulate inside the folder
	std::vector<char> nfo = Pattern(40, 24);
	std::vector<char> inner = Pattern(60, 25);
	std::vector<char> data;
	data.insert(data.end(), nfo.begin(), nfo.end());
	data.insert(data.end(), inner.begin(), inner.end());

	std::vector<char> header = {0x01, 0x04, 0x06};	// kHeader, kMainStreamsInfo, kPackInfo
	Put7zNumber(header, 0);						// pack pos
	Put7zNumber(header, 1);						// one pack stream
	header.push_back(0x09);						// kSize
	Put7zNumber(header, 100);
	header.push_back(0x00);						// kEnd (pack info)
	header.push_back(0x07);						// kUnPackInfo
	header.push_back(0x0b);						// kFolder
	Put7zNumber(header, 1);						// folder count
	header.push_back(0x00);						// external
	Put7zNumber(header, 1);						// one coder
	header.push_back(0x01);						// flags: id size 1
	header.push_back(0x00);						// Copy
	header.push_back(0x0c);						// kCodersUnpackSize
	Put7zNumber(header, 100);
	header.push_back(0x00);						// kEnd (unpack info)
	header.push_back(0x08);						// kSubStreamsInfo
	header.push_back(0x0d);						// kNumUnpackStream
	Put7zNumber(header, 2);
	header.push_back(0x09);						// kSize: all but the last substream
	Put7zNumber(header, 40);
	header.push_back(0x00);						// kEnd (substreams info)
	header.push_back(0x00);						// kEnd (streams info)
	header.push_back(0x05);						// kFilesInfo
	Put7zNumber(header, 3);
	header.push_back(0x0e);						// kEmptyStream
	Put7zNumber(header, 1);
	header.push_back((char)0x80);				// bit 0 (MSB-first): the directory
	std::vector<char> names;
	names.push_back(0x00);						// external
	for (const char* name : {"sample", "info.nfo", "movie.mkv"})
	{
		for (const char* ch = name; *ch; ch++)
		{
			names.push_back(*ch);
			names.push_back(0x00);
		}
		names.push_back(0x00);
		names.push_back(0x00);
	}
	header.push_back(0x11);						// kName
	Put7zNumber(header, names.size());
	header.insert(header.end(), names.begin(), names.end());
	header.push_back(0x00);						// end of file properties
	header.push_back(0x00);						// kEnd (header)

	std::vector<SetMember> members = {{"rel.7z", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		Wrap7zHeader(header, data)));
	MemberSet set{MemberSet::mfSevenZip, {0}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason);
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK_EQUAL(map->GetInnerName(), "movie.mkv");
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 60);
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);
	BOOST_CHECK_EQUAL((*map->GetRuns())[0].MemberOffset, 32 + 40);

	std::vector<char> reassembled(60);
	const ContentRun& run = (*map->GetRuns())[0];
	BOOST_REQUIRE(sources.GetSource(0)->Read(run.MemberOffset, reassembled.data(), 60));
	BOOST_CHECK(!memcmp(reassembled.data(), inner.data(), 60));
}

BOOST_AUTO_TEST_CASE(ContentMapperSevenZipDigestSkipTest)
{
	// realistic Copy archives carry kCRC records at every level; the parser
	// ignores the VALUES but must skip each record exactly or every field
	// after it desyncs. Folder 0 (one substream, folder crc defined) is
	// excluded from the substream digest count; folder 1 contributes its two.
	std::vector<char> nfo = Pattern(20, 26);
	std::vector<char> notes = Pattern(30, 27);
	std::vector<char> inner = Pattern(60, 28);
	std::vector<char> data;
	data.insert(data.end(), nfo.begin(), nfo.end());
	data.insert(data.end(), notes.begin(), notes.end());
	data.insert(data.end(), inner.begin(), inner.end());

	std::vector<char> header = {0x01, 0x04, 0x06};	// kHeader, kMainStreamsInfo, kPackInfo
	Put7zNumber(header, 0);						// pack pos
	Put7zNumber(header, 2);						// two pack streams
	header.push_back(0x09);						// kSize
	Put7zNumber(header, 20);					// folder 0: the nfo
	Put7zNumber(header, 90);					// folder 1: notes + movie (solid)
	header.push_back(0x0a);						// kCRC (pack): defined-bits vector
	header.push_back(0x00);						// allAreDefined = 0
	header.push_back(0x40);						// MSB-first: only stream 1 defined
	PutLe32(header, 0xdeadbeef);				// one digest to skip
	header.push_back(0x00);						// kEnd (pack info)
	header.push_back(0x07);						// kUnPackInfo
	header.push_back(0x0b);						// kFolder
	Put7zNumber(header, 2);						// folder count
	header.push_back(0x00);						// external
	for (int f = 0; f < 2; f++)
	{
		Put7zNumber(header, 1);					// one coder
		header.push_back(0x01);					// flags: id size 1
		header.push_back(0x00);					// Copy
	}
	header.push_back(0x0c);						// kCodersUnpackSize
	Put7zNumber(header, 20);
	Put7zNumber(header, 90);
	header.push_back(0x0a);						// kCRC (folders)
	header.push_back(0x01);						// allAreDefined = 1
	PutLe32(header, 0xdeadbeef);				// folder 0 digest
	PutLe32(header, 0xfeedface);				// folder 1 digest
	header.push_back(0x00);						// kEnd (unpack info)
	header.push_back(0x08);						// kSubStreamsInfo
	header.push_back(0x0d);						// kNumUnpackStream
	Put7zNumber(header, 1);						// folder 0: one substream
	Put7zNumber(header, 2);						// folder 1: two substreams
	header.push_back(0x09);						// kSize: all but the last per folder
	Put7zNumber(header, 30);					// folder 1 first substream (notes)
	header.push_back(0x0a);						// kCRC (substreams): folder 0 is
	header.push_back(0x01);						// covered (1 stream + folder crc),
	PutLe32(header, 0xdeadbeef);				// so exactly TWO digests follow
	PutLe32(header, 0xfeedface);				// (folder 1's substreams)
	header.push_back(0x00);						// kEnd (substreams info)
	header.push_back(0x00);						// kEnd (streams info)
	header.push_back(0x05);						// kFilesInfo
	Put7zNumber(header, 3);
	std::vector<char> names;
	names.push_back(0x00);						// external
	for (const char* name : {"info.nfo", "notes.txt", "movie.mkv"})
	{
		for (const char* ch = name; *ch; ch++)
		{
			names.push_back(*ch);
			names.push_back(0x00);
		}
		names.push_back(0x00);
		names.push_back(0x00);
	}
	header.push_back(0x11);						// kName
	Put7zNumber(header, names.size());
	header.insert(header.end(), names.begin(), names.end());
	header.push_back(0x00);						// end of file properties
	header.push_back(0x00);						// kEnd (header)

	std::vector<SetMember> members = {{"rel.7z", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		Wrap7zHeader(header, data)));
	MemberSet set{MemberSet::mfSevenZip, {0}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason);
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK_EQUAL(map->GetInnerName(), "movie.mkv");
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 60);
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);
	// movie data sits after the nfo folder (20) and the notes substream (30)
	BOOST_CHECK_EQUAL((*map->GetRuns())[0].MemberOffset, 32 + 20 + 30);

	std::vector<char> reassembled(60);
	const ContentRun& run = (*map->GetRuns())[0];
	BOOST_REQUIRE(sources.GetSource(0)->Read(run.MemberOffset, reassembled.data(), 60));
	BOOST_CHECK(!memcmp(reassembled.data(), inner.data(), 60));
}

BOOST_AUTO_TEST_CASE(HoledSourceTest)
{
	MemoryContentSource inner(Pattern(100, 17));
	HoledSource holed(inner, {{30, 10}});

	char buffer[100];
	BOOST_CHECK(holed.Read(0, buffer, 30));		// clear of the hole
	BOOST_CHECK(holed.Read(40, buffer, 60));	// clear of the hole
	BOOST_CHECK(!holed.Read(25, buffer, 10));	// straddles the hole start
	BOOST_CHECK(!holed.Read(35, buffer, 1));	// inside the hole
	BOOST_CHECK(!holed.Read(0, buffer, 100));	// spans the hole entirely
	BOOST_CHECK_EQUAL(holed.Size(), 100);
}

BOOST_AUTO_TEST_CASE(ContentMapperBuildRepairSetsTest)
{
	// a 3-volume store rar (40/40/20 of a 100-byte inner file) + a bare file
	std::vector<char> inner = Pattern(100, 18);
	std::vector<char> slice0(inner.begin(), inner.begin() + 40);
	std::vector<char> slice1(inner.begin() + 40, inner.begin() + 80);
	std::vector<char> slice2(inner.begin() + 80, inner.end());

	std::vector<char> vol0 = BuildRar3StoreVolume(slice0, 100, false, true);
	std::vector<char> vol1 = BuildRar3StoreVolume(slice1, 100, true, true);
	std::vector<char> vol2 = BuildRar3StoreVolume(slice2, 100, true, false);
	// per BuildRar3StoreVolume's layout the data region starts at
	// 7 (sig) + 13 (main) + 32 + 9 ("inner.mkv") = 61
	const int64 dataStart = 61;

	std::vector<SetMember> members = {
		{"rel.part01.rar", (int64)vol0.size()},
		{"rel.part02.rar", (int64)vol1.size()},
		{"rel.part03.rar", (int64)vol2.size()},
		{"bare.mkv", 50}};

	MemorySourceSet raw;
	raw.Sources.push_back(std::make_unique<MemoryContentSource>(vol0));
	raw.Sources.push_back(std::make_unique<MemoryContentSource>(vol1));
	raw.Sources.push_back(std::make_unique<MemoryContentSource>(vol2));
	raw.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(50, 19)));

	// vol2: a data hole (repairable); vol3: a header hole (that volume
	// degrades alone); bare: a plain hole (identity map)
	std::vector<StreamRangeList> memberHoles(4);
	memberHoles[1] = {{dataStart + 5, 10}};
	memberHoles[2] = {{0, 20}};
	memberHoles[3] = {{10, 5}};

	HoledSourceSet sources(raw, memberHoles);
	std::vector<RepairSetData> sets =
		ContentMapper::BuildRepairSets(members, memberHoles, sources);
	BOOST_REQUIRE_EQUAL(sets.size(), 2u);

	// the rar set maps with volume 3 excluded (its holes stay for par2)
	BOOST_REQUIRE_MESSAGE(sets[0].Map, sets[0].SkipReason);
	BOOST_CHECK_EQUAL(sets[0].Map->GetInnerSize(), 100);
	BOOST_CHECK_EQUAL(sets[0].Map->GetRuns()->size(), 2u);
	BOOST_REQUIRE_EQUAL(sets[0].InnerHoles.size(), 1u);
	BOOST_CHECK_EQUAL(sets[0].InnerHoles[0].Offset, 45);	// 40 + (66-61)
	BOOST_CHECK_EQUAL(sets[0].InnerHoles[0].Size, 10);

	// the build-time snapshot equals InnerHoles until a donor patches:
	// identity probes anchor to it after InnerHoles starts shrinking
	BOOST_REQUIRE_EQUAL(sets[0].OriginalInnerHoles.size(), sets[0].InnerHoles.size());
	BOOST_CHECK_EQUAL(sets[0].OriginalInnerHoles[0].Offset, sets[0].InnerHoles[0].Offset);
	BOOST_CHECK_EQUAL(sets[0].OriginalInnerHoles[0].Size, sets[0].InnerHoles[0].Size);

	// the bare file is its own identity-mapped set
	BOOST_REQUIRE_MESSAGE(sets[1].Map, sets[1].SkipReason);
	BOOST_CHECK_EQUAL(sets[1].Map->GetInnerSize(), 50);
	BOOST_REQUIRE_EQUAL(sets[1].InnerHoles.size(), 1u);
	BOOST_CHECK_EQUAL(sets[1].InnerHoles[0].Offset, 10);
	BOOST_REQUIRE_EQUAL(sets[1].OriginalInnerHoles.size(), sets[1].InnerHoles.size());
	BOOST_CHECK_EQUAL(sets[1].OriginalInnerHoles[0].Offset, sets[1].InnerHoles[0].Offset);
	BOOST_CHECK_EQUAL(sets[1].OriginalInnerHoles[0].Size, sets[1].InnerHoles[0].Size);

	// an unmappable set still reports (compressed single-volume rar)
	{
		std::vector<SetMember> compressedMembers = {{"c.rar", 0}};
		MemorySourceSet compressedRaw;
		compressedRaw.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3StoreVolume(slice0, 100, false, false, "inner.mkv", 0x33)));
		std::vector<StreamRangeList> compressedHoles = {{{70, 5}}};
		HoledSourceSet compressedSources(compressedRaw, compressedHoles);
		std::vector<RepairSetData> compressedSets = ContentMapper::BuildRepairSets(
			compressedMembers, compressedHoles, compressedSources);
		BOOST_REQUIRE_EQUAL(compressedSets.size(), 1u);
		BOOST_CHECK(!compressedSets[0].Map);
		BOOST_CHECK(!compressedSets[0].SkipReason.empty());
	}

	// sets without holes never appear
	{
		std::vector<StreamRangeList> noHoles(4);
		HoledSourceSet cleanSources(raw, noHoles);
		BOOST_CHECK(ContentMapper::BuildRepairSets(members, noHoles, cleanSources).empty());
	}
}

BOOST_AUTO_TEST_CASE(ContentMapperCoalesceRangesTest)
{
	// empty input
	BOOST_CHECK(ContentMapper::CoalesceRanges({}).empty());

	// disjoint ranges are preserved (and sorted by offset)
	StreamRangeList disjoint = ContentMapper::CoalesceRanges({{100, 10}, {0, 10}});
	BOOST_REQUIRE_EQUAL(disjoint.size(), 2u);
	BOOST_CHECK_EQUAL(disjoint[0].Offset, 0);
	BOOST_CHECK_EQUAL(disjoint[0].Size, 10);
	BOOST_CHECK_EQUAL(disjoint[1].Offset, 100);
	BOOST_CHECK_EQUAL(disjoint[1].Size, 10);

	// overlap merges
	StreamRangeList overlap = ContentMapper::CoalesceRanges({{0, 20}, {10, 30}});
	BOOST_REQUIRE_EQUAL(overlap.size(), 1u);
	BOOST_CHECK_EQUAL(overlap[0].Offset, 0);
	BOOST_CHECK_EQUAL(overlap[0].Size, 40);

	// adjacency (end == offset) merges; a contained range never widens
	StreamRangeList adjacent = ContentMapper::CoalesceRanges(
		{{0, 10}, {10, 5}, {2, 3}});
	BOOST_REQUIRE_EQUAL(adjacent.size(), 1u);
	BOOST_CHECK_EQUAL(adjacent[0].Offset, 0);
	BOOST_CHECK_EQUAL(adjacent[0].Size, 15);

	// the double-count breach geometry: original holes [0,100000) and
	// [100040,200000) hug the SAME 40-byte island from both sides, so
	// window building emits {100000,40} twice - coalesced they must count
	// 40 comparable bytes ONCE, keeping the 64-byte identity floor honest
	StreamRangeList windows = ContentMapper::CoalesceRanges(
		{{100000, 40}, {100000, 40}});
	BOOST_REQUIRE_EQUAL(windows.size(), 1u);
	BOOST_CHECK_EQUAL(windows[0].Offset, 100000);
	BOOST_CHECK_EQUAL(windows[0].Size, 40);
	int64 total = 0;
	for (const StreamRange& window : windows)
	{
		total += window.Size;
	}
	BOOST_CHECK_EQUAL(total, 40);
}

#ifndef DISABLE_TLS

namespace
{

// pad plaintext to the next 16-byte boundary and AES-CBC encrypt it with `ctx`
// (chain-start = header IV): the on-disk ciphertext a store-rar volume carries
std::vector<char> EncryptPadded(RarCryptoContext& ctx, const std::vector<char>& plain)
{
	int64 padded = ((int64)plain.size() + 15) / 16 * 16;
	std::vector<uint8> in((size_t)padded, 0);
	memcpy(in.data(), plain.data(), plain.size());
	std::vector<uint8> out((size_t)padded);
	BOOST_REQUIRE(ctx.EncryptRange(nullptr, in.data(), out.data(), padded / 16));
	return std::vector<char>(out.begin(), out.end());
}

std::vector<char> SlurpFile(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	return std::vector<char>((std::istreambuf_iterator<char>(in)),
		std::istreambuf_iterator<char>());
}

}

// Happy path: a 3-volume RAR3 -p store archive maps in PLAINTEXT space. The
// runs span the plaintext file, every run is annotated with the SAME crypto
// context (one continuous CBC stream), and decrypting the concatenated on-disk
// ciphertext through that context reproduces the original plaintext byte for
// byte. Non-last cipher chunks are 16-aligned; the last carries the padded tail.
BOOST_AUTO_TEST_CASE(ContentMapperRar3EncryptedStoreMapTest)
{
	std::vector<char> inner = Pattern(100, 5);
	const uint8 salt[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	auto ctx = RarCryptoContext::MakeRar3("123", salt);
	BOOST_REQUIRE(ctx != nullptr);

	// cipher = AES-CBC(pad16(inner)) = 112 bytes; split 48 | 48 | 16 (16-aligned
	// cuts). plainLast = 100 - 96 = 4, padded to 16 in the last volume.
	std::vector<char> cipher = EncryptPadded(*ctx, inner);
	BOOST_REQUIRE_EQUAL(cipher.size(), 112u);
	std::vector<char> c0(cipher.begin(), cipher.begin() + 48);
	std::vector<char> c1(cipher.begin() + 48, cipher.begin() + 96);
	std::vector<char> c2(cipher.begin() + 96, cipher.end());

	std::vector<SetMember> members = {
		{"rel.part01.rar", 0}, {"rel.part02.rar", 0}, {"rel.part03.rar", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c0, 100, false, true, salt)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c1, 100, true, true, salt)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c2, 100, true, false, salt)));

	MemberSet set{MemberSet::mfRar, {0, 1, 2}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason, "123");
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK_EQUAL(map->GetInnerName(), "inner.mkv");
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 100);
	BOOST_CHECK(map->GetEncrypted());
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 3u);

	// plaintext run geometry: 48 + 48 + 4 = 100
	BOOST_CHECK_EQUAL((*map->GetRuns())[0].InnerOffset, 0);
	BOOST_CHECK_EQUAL((*map->GetRuns())[0].Size, 48);
	BOOST_CHECK_EQUAL((*map->GetRuns())[1].InnerOffset, 48);
	BOOST_CHECK_EQUAL((*map->GetRuns())[1].Size, 48);
	BOOST_CHECK_EQUAL((*map->GetRuns())[2].InnerOffset, 96);
	BOOST_CHECK_EQUAL((*map->GetRuns())[2].Size, 4);

	// every run shares the one stream context; CipherDataOffset == the member
	// offset where that volume's ciphertext begins
	const RunCrypto* rc0 = map->GetRunCrypto(0);
	BOOST_REQUIRE(rc0 && rc0->Crypto);
	for (size_t i = 0; i < map->GetRuns()->size(); i++)
	{
		const RunCrypto* rc = map->GetRunCrypto(i);
		BOOST_REQUIRE(rc && rc->Crypto);
		BOOST_CHECK(rc->Crypto == rc0->Crypto);
		BOOST_CHECK_EQUAL(rc->CipherDataOffset, (*map->GetRuns())[i].MemberOffset);
	}

	// reassemble the ciphertext (in inner order) and decrypt through the map's
	// context: the first innerSize plaintext bytes must equal the original
	std::vector<uint8> full(cipher.begin(), cipher.end());
	std::vector<uint8> plainOut(full.size());
	BOOST_REQUIRE(rc0->Crypto->DecryptRange(nullptr, full.data(), plainOut.data(),
		(int64)full.size() / 16));
	BOOST_CHECK(!memcmp(plainOut.data(), inner.data(), 100));
}

// Happy path: a single-volume RAR5 -p store archive (with a password-check
// value). The whole file is one CBC stream; PackedSize == ceil16(innerSize).
BOOST_AUTO_TEST_CASE(ContentMapperRar5EncryptedStoreMapTest)
{
	std::vector<char> inner = Pattern(50, 9);
	const uint8 salt[16] = {
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
	const uint8 iv[16] = {
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf};
	const uint8 kdf = 12;
	uint8 check[12] = {};
	BOOST_REQUIRE(StreamCrypto::DeriveRar5PswCheck("123", kdf, salt, check));

	RarFile::Rar5Crypt crypt;
	crypt.Version = 0;
	crypt.KdfCount = kdf;
	memcpy(crypt.Salt, salt, 16);
	memcpy(crypt.Iv, iv, 16);
	memcpy(crypt.CheckValue, check, 8);
	crypt.HasCheck = true;
	auto ctx = RarCryptoContext::MakeRar5("123", crypt);
	BOOST_REQUIRE(ctx != nullptr);

	std::vector<char> cipher = EncryptPadded(*ctx, inner);	// 64 bytes = ceil16(50)
	BOOST_REQUIRE_EQUAL(cipher.size(), 64u);

	std::vector<SetMember> members = {{"rel.part01.rar", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar5EncStoreVolume(cipher, 50, false, false, kdf, salt, iv, check)));

	MemberSet set{MemberSet::mfRar, {0}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason, "123");
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 50);
	BOOST_CHECK(map->GetEncrypted());
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 1u);
	BOOST_CHECK_EQUAL((*map->GetRuns())[0].Size, 50);

	const RunCrypto* rc = map->GetRunCrypto(0);
	BOOST_REQUIRE(rc && rc->Crypto);
	std::vector<uint8> plainOut(cipher.size());
	BOOST_REQUIRE(rc->Crypto->DecryptRange(nullptr, (const uint8*)cipher.data(),
		plainOut.data(), (int64)cipher.size() / 16));
	BOOST_CHECK(!memcmp(plainOut.data(), inner.data(), 50));
}

// The rejection matrix: wrong password, no password (M2 behavior preserved),
// and the fail-closed geometry gates.
BOOST_AUTO_TEST_CASE(ContentMapperRarEncryptedRejectionsTest)
{
	std::vector<char> inner = Pattern(50, 3);
	const uint8 salt3[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	const uint8 salt5[16] = {
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
	const uint8 iv5[16] = {
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
	const uint8 kdf = 11;
	uint8 check[12] = {};
	BOOST_REQUIRE(StreamCrypto::DeriveRar5PswCheck("123", kdf, salt5, check));

	auto ctx3 = RarCryptoContext::MakeRar3("123", salt3);
	std::vector<char> cipher3 = EncryptPadded(*ctx3, inner);	// 64 bytes

	MemberSet single{MemberSet::mfRar, {0}};
	std::vector<SetMember> members = {{"rel.part01.rar", 0}};
	std::string skipReason;

	// wrong RAR5 password: the stored check rejects it, fail closed
	{
		RarFile::Rar5Crypt crypt;
		crypt.Version = 0; crypt.KdfCount = kdf;
		memcpy(crypt.Salt, salt5, 16); memcpy(crypt.Iv, iv5, 16);
		memcpy(crypt.CheckValue, check, 8); crypt.HasCheck = true;
		auto ctx5 = RarCryptoContext::MakeRar5("123", crypt);
		std::vector<char> cipher5 = EncryptPadded(*ctx5, inner);
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar5EncStoreVolume(cipher5, 50, false, false, kdf, salt5, iv5, check)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason, "wrongpw"));
		BOOST_CHECK_EQUAL(skipReason, "archive password rejected");
	}

	// no password: M2 behavior is unchanged (still "encrypted archive data")
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(cipher3, 50, false, false, salt3)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason));
		BOOST_CHECK_EQUAL(skipReason, "encrypted archive data");
	}

	// geometry LIFT (M3 Task 4): a non-16-aligned NON-last volume is how real
	// multi-volume RAR actually splits; the contiguous cipher-space model maps
	// it as long as the slabs total exactly ceil16(innerSize)
	{
		std::vector<char> c0(cipher3.begin(), cipher3.begin() + 47);	// 47 % 16 != 0
		std::vector<char> c1(cipher3.begin() + 47, cipher3.end());		// 17 bytes
		std::vector<SetMember> pair = {{"rel.part01.rar", 0}, {"rel.part02.rar", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c0, 50, false, true, salt3)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c1, 50, true, false, salt3)));
		MemberSet twoVol{MemberSet::mfRar, {0, 1}};
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(pair, twoVol, sources, skipReason, "123");
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 2u);
		BOOST_CHECK_EQUAL((*map->GetRuns())[0].Size, 47);
		BOOST_CHECK_EQUAL((*map->GetRuns())[1].Size, 3);	// plaintext tail of 50
		BOOST_CHECK_EQUAL(map->GetRunCrypto(0)->CipherSize, 47);
		BOOST_CHECK_EQUAL(map->GetRunCrypto(1)->CipherSize, 17);
		BOOST_CHECK_EQUAL(map->GetCipherStreamSize(), 64);
	}

	// geometry: slabs that do NOT total ceil16(innerSize) (a truncated or
	// foreign last volume) fail closed
	{
		std::vector<char> c0(cipher3.begin(), cipher3.begin() + 47);
		std::vector<char> c1(cipher3.begin() + 47, cipher3.begin() + 60);	// 47+13=60 != 64
		std::vector<SetMember> pair = {{"rel.part01.rar", 0}, {"rel.part02.rar", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c0, 50, false, true, salt3)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c1, 50, true, false, salt3)));
		MemberSet twoVol{MemberSet::mfRar, {0, 1}};
		BOOST_CHECK(!ContentMapper::BuildMap(pair, twoVol, sources, skipReason, "123"));
		BOOST_CHECK_EQUAL(skipReason, "encrypted volume geometry does not fit store mode");
	}

	// geometry: a padding-only last volume (its slab starts at or past the
	// plaintext end) - no real RAR splits inside the padding, fail closed
	{
		std::vector<char> c0(cipher3.begin(), cipher3.begin() + 52);	// 52 >= innerSize 50
		std::vector<char> c1(cipher3.begin() + 52, cipher3.end());		// 12 bytes, total 64
		std::vector<SetMember> pair = {{"rel.part01.rar", 0}, {"rel.part02.rar", 0}};
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c0, 50, false, true, salt3)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(c1, 50, true, false, salt3)));
		MemberSet twoVol{MemberSet::mfRar, {0, 1}};
		BOOST_CHECK(!ContentMapper::BuildMap(pair, twoVol, sources, skipReason, "123"));
		BOOST_CHECK_EQUAL(skipReason, "encrypted volume geometry does not fit store mode");
	}

	// geometry: a single volume whose packed size is not ceil16(innerSize)
	{
		std::vector<char> shortCipher(cipher3.begin(), cipher3.begin() + 60);	// not 64
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar3EncStoreVolume(shortCipher, 50, false, false, salt3)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, sources, skipReason, "123"));
		BOOST_CHECK_EQUAL(skipReason, "encrypted volume geometry does not fit store mode");
	}
}

// The cipher composite (M3 Task 4 gate lift): a 3-volume RAR3 -p store archive
// with ARBITRARY (non-16-aligned) volume cuts - the geometry real WinRAR
// produces - maps as one contiguous cipher space. Cross-member 16-byte block
// assembly must reproduce the plaintext byte for byte, and every degradation
// (uncovered member, out-of-range request) must fail closed.
BOOST_AUTO_TEST_CASE(ContentMapperRarEncryptedCompositeTest)
{
	std::vector<char> inner = Pattern(100, 41);
	const uint8 salt[8] = {0x9a, 0x8b, 0x7c, 0x6d, 0x5e, 0x4f, 0x30, 0x21};
	auto ctx = RarCryptoContext::MakeRar3("123", salt);
	BOOST_REQUIRE(ctx != nullptr);

	// one continuous CBC stream of ceil16(100) = 112 bytes, cut at 45 | 37 | 30:
	// every cut lands mid-block, so target blocks and their predecessors
	// straddle member boundaries
	std::vector<char> cipher = EncryptPadded(*ctx, inner);
	BOOST_REQUIRE_EQUAL(cipher.size(), 112u);
	std::vector<char> c0(cipher.begin(), cipher.begin() + 45);
	std::vector<char> c1(cipher.begin() + 45, cipher.begin() + 82);
	std::vector<char> c2(cipher.begin() + 82, cipher.end());

	std::vector<SetMember> members = {
		{"rel.part01.rar", 0}, {"rel.part02.rar", 0}, {"rel.part03.rar", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c0, 100, false, true, salt)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c1, 100, true, true, salt)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c2, 100, true, false, salt)));

	MemberSet set{MemberSet::mfRar, {0, 1, 2}};
	std::string skipReason;
	std::unique_ptr<ContentMap> map =
		ContentMapper::BuildMap(members, set, sources, skipReason, "123");
	BOOST_REQUIRE_MESSAGE(map, skipReason);
	BOOST_CHECK(map->GetEncrypted());
	BOOST_CHECK_EQUAL(map->GetInnerSize(), 100);
	BOOST_CHECK_EQUAL(map->GetCipherStreamSize(), 112);

	// plaintext runs 45 + 37 + 18 (the last slab's 12 padding bytes carry no
	// plaintext); slabs 45 + 37 + 30; one shared context
	BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 3u);
	const int64 runSizes[] = {45, 37, 18};
	const int64 slabSizes[] = {45, 37, 30};
	const RunCrypto* rc0 = map->GetRunCrypto(0);
	BOOST_REQUIRE(rc0 && rc0->Crypto);
	for (size_t i = 0; i < 3; i++)
	{
		const ContentRun& run = (*map->GetRuns())[i];
		const RunCrypto* rc = map->GetRunCrypto(i);
		BOOST_REQUIRE(rc && rc->Crypto);
		BOOST_CHECK(rc->Crypto == rc0->Crypto);
		BOOST_CHECK_EQUAL(run.Size, runSizes[i]);
		BOOST_CHECK_EQUAL(rc->CipherSize, slabSizes[i]);
		BOOST_CHECK_EQUAL(rc->CipherDataOffset, run.MemberOffset);
	}

	// cipher-space translation: [32, 64) straddles the vol0/vol1 cut at 45
	int64 dataOff0 = (*map->GetRuns())[0].MemberOffset;
	int64 dataOff1 = (*map->GetRuns())[1].MemberOffset;
	std::vector<MemberRange> pieces = map->MapCipherRange({32, 32});
	BOOST_REQUIRE_EQUAL(pieces.size(), 2u);
	BOOST_CHECK_EQUAL(pieces[0].MemberIndex, 0);
	BOOST_CHECK_EQUAL(pieces[0].Range.Offset, dataOff0 + 32);
	BOOST_CHECK_EQUAL(pieces[0].Range.Size, 13);
	BOOST_CHECK_EQUAL(pieces[1].MemberIndex, 1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Offset, dataOff1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Size, 19);

	// out-of-stream and degenerate requests fail closed
	BOOST_CHECK(map->MapCipherRange({0, 113}).empty());
	BOOST_CHECK(map->MapCipherRange({112, 16}).empty());
	BOOST_CHECK(map->MapCipherRange({-1, 4}).empty());

	// non-aligned reassembly is byte-identical to the plaintext: whole file,
	// cut-straddling ranges, the padded tail block, single bytes
	std::vector<char> out(100);
	BOOST_REQUIRE(map->ReadInnerDecrypted(sources, {0, 100}, out.data()));
	BOOST_CHECK(!memcmp(out.data(), inner.data(), 100));
	const StreamRange probes[] = {{40, 10}, {80, 10}, {95, 5}, {0, 1}, {99, 1}, {44, 3}};
	for (const StreamRange& probe : probes)
	{
		std::vector<char> piece(probe.Size);
		BOOST_REQUIRE(map->ReadInnerDecrypted(sources, probe, piece.data()));
		BOOST_CHECK(!memcmp(piece.data(), inner.data() + probe.Offset, probe.Size));
	}
	BOOST_CHECK(map->ReadInnerDecrypted(sources, {50, 0}, out.data()));	// no-op
	BOOST_CHECK(!map->ReadInnerDecrypted(sources, {96, 10}, out.data()));	// past the end
	BOOST_CHECK(!map->ReadInnerDecrypted(sources, {-16, 16}, out.data()));

	// adversarial: a member missing on the source side (donor lost a volume) -
	// any block touching it, incl. as a predecessor, must fail, while ranges
	// served entirely by present members still work
	MemorySourceSet partial;
	partial.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c0, 100, false, true, salt)));
	partial.Sources.push_back(nullptr);
	partial.Sources.push_back(std::make_unique<MemoryContentSource>(
		BuildRar3EncStoreVolume(c2, 100, true, false, salt)));
	BOOST_CHECK(map->ReadInnerDecrypted(partial, {0, 32}, out.data()));
	BOOST_CHECK(!memcmp(out.data(), inner.data(), 32));
	BOOST_CHECK(!map->ReadInnerDecrypted(partial, {50, 10}, out.data()));	// inside vol1
	BOOST_CHECK(!map->ReadInnerDecrypted(partial, {0, 48}, out.data()));	// block tail in vol1
	BOOST_CHECK(!map->ReadInnerDecrypted(partial, {96, 4}, out.data()));	// predecessor in vol1
}

// The same lift for RAR5: two volumes cut mid-block; the crypt-record
// agreement gate still applies across the non-aligned cut.
BOOST_AUTO_TEST_CASE(ContentMapperRar5EncryptedNonAlignedTest)
{
	std::vector<char> inner = Pattern(60, 27);
	const uint8 salt[16] = {
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf};
	const uint8 iv[16] = {
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf};
	const uint8 kdf = 10;
	uint8 check[12] = {};
	BOOST_REQUIRE(StreamCrypto::DeriveRar5PswCheck("123", kdf, salt, check));

	RarFile::Rar5Crypt crypt;
	crypt.Version = 0;
	crypt.KdfCount = kdf;
	memcpy(crypt.Salt, salt, 16);
	memcpy(crypt.Iv, iv, 16);
	memcpy(crypt.CheckValue, check, 8);
	crypt.HasCheck = true;
	auto ctx = RarCryptoContext::MakeRar5("123", crypt);
	BOOST_REQUIRE(ctx != nullptr);

	std::vector<char> cipher = EncryptPadded(*ctx, inner);	// 64 = ceil16(60)
	std::vector<char> c0(cipher.begin(), cipher.begin() + 23);	// 23 % 16 != 0
	std::vector<char> c1(cipher.begin() + 23, cipher.end());	// 41 bytes

	std::vector<SetMember> members = {{"rel.part01.rar", 0}, {"rel.part02.rar", 0}};
	MemberSet set{MemberSet::mfRar, {0, 1}};
	std::string skipReason;
	{
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar5EncStoreVolume(c0, 60, false, true, kdf, salt, iv, check)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar5EncStoreVolume(c1, 60, true, false, kdf, salt, iv, check)));
		std::unique_ptr<ContentMap> map =
			ContentMapper::BuildMap(members, set, sources, skipReason, "123");
		BOOST_REQUIRE_MESSAGE(map, skipReason);
		BOOST_REQUIRE_EQUAL(map->GetRuns()->size(), 2u);
		BOOST_CHECK_EQUAL((*map->GetRuns())[0].Size, 23);
		BOOST_CHECK_EQUAL((*map->GetRuns())[1].Size, 37);
		BOOST_CHECK_EQUAL(map->GetRunCrypto(1)->CipherSize, 41);

		std::vector<char> out(60);
		BOOST_REQUIRE(map->ReadInnerDecrypted(sources, {0, 60}, out.data()));
		BOOST_CHECK(!memcmp(out.data(), inner.data(), 60));
		BOOST_REQUIRE(map->ReadInnerDecrypted(sources, {20, 10}, out.data()));
		BOOST_CHECK(!memcmp(out.data(), inner.data() + 20, 10));
	}

	// a diverging IV in the second volume breaks the one-stream assumption
	{
		uint8 otherIv[16];
		memcpy(otherIv, iv, 16);
		otherIv[0] ^= 0x01;
		MemorySourceSet sources;
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar5EncStoreVolume(c0, 60, false, true, kdf, salt, iv, check)));
		sources.Sources.push_back(std::make_unique<MemoryContentSource>(
			BuildRar5EncStoreVolume(c1, 60, true, false, kdf, salt, otherIv, check)));
		BOOST_CHECK(!ContentMapper::BuildMap(members, set, sources, skipReason, "123"));
		BOOST_CHECK_EQUAL(skipReason, "encrypted volume geometry does not fit store mode");
	}
}

// BuildRepairSets threads the TARGET's own password: an encrypted store-rar
// target set maps (with hole translation) when the password is supplied and
// keeps the M2 skip without one.
BOOST_AUTO_TEST_CASE(ContentMapperEncryptedRepairSetsTest)
{
	std::vector<char> inner = Pattern(50, 33);
	const uint8 salt[8] = {8, 7, 6, 5, 4, 3, 2, 1};
	auto ctx = RarCryptoContext::MakeRar3("123", salt);
	BOOST_REQUIRE(ctx != nullptr);
	std::vector<char> cipher = EncryptPadded(*ctx, inner);	// 64 bytes
	std::vector<char> volume = BuildRar3EncStoreVolume(cipher, 50, false, false, salt);
	// per BuildRar3EncStoreVolume's layout the data region starts at
	// 7 (sig) + 13 (main) + 32 + 9 ("inner.mkv") + 8 (salt) = 69
	const int64 dataStart = 69;

	std::vector<SetMember> members = {{"rel.part01.rar", (int64)volume.size()}};
	std::vector<StreamRangeList> memberHoles(1);
	memberHoles[0] = {{dataStart + 20, 10}};

	MemorySourceSet raw;
	raw.Sources.push_back(std::make_unique<MemoryContentSource>(volume));
	HoledSourceSet sources(raw, memberHoles);

	std::vector<RepairSetData> without =
		ContentMapper::BuildRepairSets(members, memberHoles, sources);
	BOOST_REQUIRE_EQUAL(without.size(), 1u);
	BOOST_CHECK(!without[0].Map);
	BOOST_CHECK_EQUAL(without[0].SkipReason, "encrypted archive data");

	std::vector<RepairSetData> with =
		ContentMapper::BuildRepairSets(members, memberHoles, sources, "123");
	BOOST_REQUIRE_EQUAL(with.size(), 1u);
	BOOST_REQUIRE_MESSAGE(with[0].Map, with[0].SkipReason);
	BOOST_CHECK(with[0].Map->GetEncrypted());
	BOOST_REQUIRE_EQUAL(with[0].InnerHoles.size(), 1u);
	BOOST_CHECK_EQUAL(with[0].InnerHoles[0].Offset, 20);
	BOOST_CHECK_EQUAL(with[0].InnerHoles[0].Size, 10);
}

// Target-side -hp safety (M3 review Should-Fix): BuildRepairSets threads the
// TARGET's own password into BuildMap -> BuildRarMap -> SetPassword
// unconditionally, so a -hp target attempts header decryption through the
// hole-aware source set. That is safe by construction: RarSourceCursor::Read
// propagates a holed read as a short/zero read, so a hole over the header
// region fails the parse (fail-closed), while a successful parse means the
// header bytes were fully primary (non-hole) - only the DATA region may be
// holed and still map. This drives that exact target-role path through a
// HoledSourceSet. The fixture is -p (encrypted DATA, plaintext headers), but
// the safety mechanism under test is the RarSourceCursor short-read, which is
// agnostic to whether the holed header bytes are -hp-encrypted or plaintext.
BOOST_AUTO_TEST_CASE(ContentMapperEncryptedTargetHeaderHoleTest)
{
	std::vector<char> inner = Pattern(50, 71);
	const uint8 salt[8] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04};
	auto ctx = RarCryptoContext::MakeRar3("123", salt);
	BOOST_REQUIRE(ctx != nullptr);
	std::vector<char> cipher = EncryptPadded(*ctx, inner);	// 64 bytes
	std::vector<char> volume = BuildRar3EncStoreVolume(cipher, 50, false, false, salt);
	// layout: 7 (sig) + 13 (main) + 32 + 9 ("inner.mkv") + 8 (salt) = 69 header
	// bytes, then 64 cipher bytes of data
	const int64 dataStart = 69;

	std::vector<SetMember> members = {{"rel.part01.rar", (int64)volume.size()}};

	// (a) a hole over the FILE-header region: the header parse cannot read past
	// it, so the target set fails closed (no map) even WITH the password
	{
		std::vector<StreamRangeList> memberHoles(1);
		memberHoles[0] = {{30, 12}};	// inside the FILE header (name/salt area)
		MemorySourceSet raw;
		raw.Sources.push_back(std::make_unique<MemoryContentSource>(volume));
		HoledSourceSet sources(raw, memberHoles);
		std::vector<RepairSetData> sets =
			ContentMapper::BuildRepairSets(members, memberHoles, sources, "123");
		BOOST_REQUIRE_EQUAL(sets.size(), 1u);
		BOOST_CHECK(!sets[0].Map);					// fail-closed: headers holed
		BOOST_CHECK(!sets[0].SkipReason.empty());
	}

	// (b) headers intact, a hole only over the DATA region: the -hp path parses
	// the (present) headers, the encrypted map builds, and the data hole
	// translates to a plaintext inner hole
	{
		std::vector<StreamRangeList> memberHoles(1);
		memberHoles[0] = {{dataStart + 16, 10}};	// plaintext inner [16, 26)
		MemorySourceSet raw;
		raw.Sources.push_back(std::make_unique<MemoryContentSource>(volume));
		HoledSourceSet sources(raw, memberHoles);
		std::vector<RepairSetData> sets =
			ContentMapper::BuildRepairSets(members, memberHoles, sources, "123");
		BOOST_REQUIRE_EQUAL(sets.size(), 1u);
		BOOST_REQUIRE_MESSAGE(sets[0].Map, sets[0].SkipReason);
		BOOST_CHECK(sets[0].Map->GetEncrypted());
		BOOST_REQUIRE_EQUAL(sets[0].InnerHoles.size(), 1u);
		BOOST_CHECK_EQUAL(sets[0].InnerHoles[0].Offset, 16);
		BOOST_CHECK_EQUAL(sets[0].InnerHoles[0].Size, 10);
	}
}

// The -hp donor decision, exercised against the REAL header-encrypted testdata
// (password "123"). Without a password the headers cannot parse and the set
// skips as encrypted headers (M2). WITH the password the SetPassword path
// decrypts the headers, so the parse succeeds and the set is judged on its
// contents instead of the header skip (these fixtures' inner file is a .dat, so
// it is rejected at the media-eligibility gate) - proving -hp donors reach the
// data-mapping path once unlocked, exactly like -p donors.
BOOST_AUTO_TEST_CASE(ContentMapperRarEncryptedHpDonorTest)
{
	const fs::path dir = fs::current_path() / "rarrenamer";
	struct Case { const char* file; };
	for (const char* file : {"testfile3encnam.part01.rar", "testfile5encnam.part01.rar"})
	{
		std::vector<char> bytes = SlurpFile((dir / file).string());
		BOOST_REQUIRE_MESSAGE(bytes.size() > 100, file);

		std::vector<SetMember> members = {{file, 0}};
		MemberSet single{MemberSet::mfRar, {0}};

		MemorySourceSet noPw;
		noPw.Sources.push_back(std::make_unique<MemoryContentSource>(bytes));
		std::string reason;
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, noPw, reason));
		BOOST_CHECK_EQUAL(reason, "encrypted archive headers");

		MemorySourceSet withPw;
		withPw.Sources.push_back(std::make_unique<MemoryContentSource>(bytes));
		std::string reason2;
		BOOST_CHECK(!ContentMapper::BuildMap(members, single, withPw, reason2, "123"));
		// the header parse succeeded (else we'd get the header skip); the set is
		// rejected on its data instead
		BOOST_CHECK_MESSAGE(reason2 != "encrypted archive headers", reason2);
		BOOST_CHECK_MESSAGE(!reason2.empty(), file);
	}
}

#endif

BOOST_AUTO_TEST_CASE(ContentMapperIsCompressibleArchiveTest)
{
	// an extractor can process these - M4's decompression donor path applies
	BOOST_CHECK(ContentMapper::IsCompressibleArchive(MemberSet{MemberSet::mfRar, {0}}));
	BOOST_CHECK(ContentMapper::IsCompressibleArchive(MemberSet{MemberSet::mfZip, {0}}));
	BOOST_CHECK(ContentMapper::IsCompressibleArchive(MemberSet{MemberSet::mfSevenZip, {0}}));

	// no compression involved - the existing store-mode (M2) paths already
	// handle these, M4 has nothing to add
	BOOST_CHECK(!ContentMapper::IsCompressibleArchive(MemberSet{MemberSet::mfBare, {0}}));
	BOOST_CHECK(!ContentMapper::IsCompressibleArchive(MemberSet{MemberSet::mfSplit, {0, 1}}));
}

BOOST_AUTO_TEST_SUITE_END()
