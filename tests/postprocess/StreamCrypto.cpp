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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <vector>
#include <cstring>
#include <boost/test/unit_test.hpp>
#include "RarReader.h"
#include "StreamCrypto.h"
#include "ContentMap.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

namespace
{

class MemoryRarSource : public ContentSource
{
public:
	MemoryRarSource(std::vector<char> data) : m_data(std::move(data)) {}
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

// A rar3 FILE volume that sets the SALT flag (0x0400) and appends `saltLen`
// salt bytes after the name. HEAD_SIZE always advertises 8 salt bytes so the
// parser expects a full salt; passing saltLen < 8 (and no trailing data) models
// a physically truncated salt.
std::vector<char> BuildRar3SaltVolume(const std::vector<char>& data,
	const char* name, const uint8* salt, int saltLen, bool trailing = true)
{
	std::vector<char> out;
	const char sig[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
	out.insert(out.end(), sig, sig + 7);

	// MAIN: VOLUME | NEWNUMBERING
	PutLe16(out, 0);
	out.push_back(0x73);
	PutLe16(out, 0x0011);
	PutLe16(out, 13);
	out.insert(out.end(), 6, 0);

	uint16 nameLen = (uint16)strlen(name);
	PutLe16(out, 0);
	out.push_back(0x74);
	PutLe16(out, (uint16)(0x8000 | 0x0400 | 0x0004));	// ADDSIZE | SALT | PASSWORD
	PutLe16(out, (uint16)(32 + nameLen + 8));			// HEAD_SIZE always claims 8 salt bytes
	PutLe32(out, (uint32)data.size());					// PACK_SIZE
	PutLe32(out, (uint32)data.size());					// UNP_SIZE
	out.push_back(0);									// HOST_OS
	PutLe32(out, 0);									// FILE_CRC
	PutLe32(out, 0);									// FTIME
	out.push_back(29);									// UNP_VER
	out.push_back(0x30);								// METHOD store
	PutLe16(out, nameLen);
	PutLe32(out, 0x20);									// ATTR
	out.insert(out.end(), name, name + nameLen);
	out.insert(out.end(), salt, salt + saltLen);

	if (!trailing)
	{
		return out;	// ends inside the salt region: the salt read runs off the end
	}

	out.insert(out.end(), data.begin(), data.end());

	PutLe16(out, 0);
	out.push_back(0x7b);
	PutLe16(out, 0);
	PutLe16(out, 7);

	return out;
}

struct Rar5CryptSpec
{
	uint64 Version = 0;
	uint64 Flags = 0x01;		// password-check present
	uint8 KdfCount = 15;
	bool IncludeSalt = true;
	bool IncludeIv = true;
	bool IncludeCheck = true;
	int RecordSizeOverride = -1;	// -1 = honest size
	bool Trailing = true;			// false = cut the volume right after the crypt bytes
};

// A rar5 FILE volume carrying an FHEXTRA_CRYPT record in its file-header extra
// area. `saltOut` (if given) receives the absolute offset of the crypt salt in
// the returned buffer so a caller can truncate the volume mid-salt.
std::vector<char> BuildRar5CryptVolume(const std::vector<char>& data,
	const char* name, const Rar5CryptSpec& spec,
	const uint8* salt, const uint8* iv, const uint8* check,
	size_t* saltOut = nullptr)
{
	// crypt record body after the type field
	std::vector<char> content;
	PutVint(content, spec.Version);
	PutVint(content, spec.Flags);
	content.push_back((char)spec.KdfCount);
	size_t saltInContent = content.size();
	if (spec.IncludeSalt) content.insert(content.end(), salt, salt + 16);
	if (spec.IncludeIv) content.insert(content.end(), iv, iv + 16);
	if (spec.IncludeCheck) content.insert(content.end(), check, check + 12);

	std::vector<char> record;
	PutVint(record, 0x01);	// record type: FHEXTRA_CRYPT
	size_t saltInRecord = record.size() + saltInContent;
	record.insert(record.end(), content.begin(), content.end());

	uint64 recordSize = spec.RecordSizeOverride >= 0 ?
		(uint64)spec.RecordSizeOverride : (uint64)record.size();

	std::vector<char> extra;
	PutVint(extra, recordSize);
	size_t saltInExtra = extra.size() + saltInRecord;
	extra.insert(extra.end(), record.begin(), record.end());

	std::vector<char> fh;
	PutVint(fh, 2);							// type: file
	PutVint(fh, 0x01 | 0x02);				// block flags: HAS_EXTRA | HAS_DATA
	PutVint(fh, extra.size());				// extra area size
	PutVint(fh, data.size());				// data area size
	PutVint(fh, 0);							// file flags (no mtime/crc)
	PutVint(fh, data.size());				// unpacked size
	PutVint(fh, 0);							// attributes
	PutVint(fh, 0);							// compression info: store
	PutVint(fh, 0);							// host os
	PutVint(fh, strlen(name));
	fh.insert(fh.end(), name, name + strlen(name));
	size_t extraInFh = fh.size();
	fh.insert(fh.end(), extra.begin(), extra.end());

	std::vector<char> out;
	const char sig[] = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00};
	out.insert(out.end(), sig, sig + 8);

	std::vector<char> mh;
	PutVint(mh, 1);
	PutVint(mh, 0);
	PutVint(mh, 0x01);
	PutLe32(out, 0);
	PutVint(out, mh.size());
	out.insert(out.end(), mh.begin(), mh.end());

	PutLe32(out, 0);
	PutVint(out, fh.size());
	size_t fhStart = out.size();
	out.insert(out.end(), fh.begin(), fh.end());
	if (saltOut) *saltOut = fhStart + extraInFh + saltInExtra;
	out.insert(out.end(), data.begin(), data.end());

	if (!spec.Trailing)
	{
		return out;
	}

	std::vector<char> eh;
	PutVint(eh, 5);
	PutVint(eh, 0);
	PutVint(eh, 0);
	PutLe32(out, 0);
	PutVint(out, eh.size());
	out.insert(out.end(), eh.begin(), eh.end());

	return out;
}

std::vector<char> Bytes(int n, uint8 seed)
{
	std::vector<char> v(n);
	for (int i = 0; i < n; i++) v[i] = (char)(seed + i * 3);
	return v;
}

}

#ifndef DISABLE_TLS

// The byte-identical-KDF regression net. These salts come from the real
// encrypted testdata; the expected key/IV bytes were computed with an
// independent reference implementation of the rar3/rar5 KDFs and confirmed to
// decrypt the actual WinRAR ciphertext in those volumes. If the extraction ever
// diverges by a single byte (swizzle, IV sampling, PBKDF2 params), this fails.
BOOST_AUTO_TEST_CASE(StreamCryptoDeriveRar3VectorTest)
{
	const uint8 salt[8] = {0x92, 0x80, 0x37, 0x09, 0x6e, 0x88, 0x92, 0xdb};
	const uint8 expectKey[16] = {
		0x56, 0xb3, 0x08, 0x9b, 0xf3, 0x26, 0x3f, 0xfa,
		0x21, 0xae, 0x16, 0x43, 0x5e, 0x80, 0x72, 0xf6};
	const uint8 expectIv[16] = {
		0x8c, 0x84, 0x7e, 0xfd, 0x84, 0x80, 0xe0, 0x41,
		0x95, 0x50, 0xee, 0x18, 0xb6, 0x88, 0x61, 0xff};

	uint8 key[16] = {};
	uint8 iv[16] = {};
	BOOST_REQUIRE(StreamCrypto::DeriveRar3("123", salt, key, iv));
	BOOST_CHECK_EQUAL_COLLECTIONS(key, key + 16, expectKey, expectKey + 16);
	BOOST_CHECK_EQUAL_COLLECTIONS(iv, iv + 16, expectIv, expectIv + 16);

	// empty password is rejected (no key material)
	BOOST_CHECK(!StreamCrypto::DeriveRar3("", salt, key, iv));
}

BOOST_AUTO_TEST_CASE(StreamCryptoDeriveRar5VectorTest)
{
	const uint8 salt[16] = {
		0x99, 0xaa, 0x76, 0x9b, 0x5d, 0x9c, 0x20, 0x19,
		0xbd, 0x73, 0xb1, 0x72, 0xb1, 0x51, 0xcf, 0xe9};
	const uint8 expectKey[32] = {
		0x26, 0x53, 0xc2, 0x2b, 0xb5, 0x33, 0x32, 0xa9,
		0x77, 0xc1, 0x4b, 0x7e, 0x1b, 0x9b, 0xc0, 0xdc,
		0x54, 0xb8, 0x51, 0x26, 0x3d, 0xd7, 0xf9, 0x87,
		0xc1, 0xb0, 0x5f, 0x26, 0xcd, 0x33, 0xe7, 0x00};

	uint8 key[32] = {};
	BOOST_REQUIRE(StreamCrypto::DeriveRar5("123", 15, salt, key));
	BOOST_CHECK_EQUAL_COLLECTIONS(key, key + 32, expectKey, expectKey + 32);

	// oversize kdfCount is rejected (fail closed, no absurd iteration count)
	uint8 dummy[32] = {};
	BOOST_CHECK(!StreamCrypto::DeriveRar5("123", 25, salt, dummy));
	BOOST_CHECK(!StreamCrypto::DeriveRar5("123", 200, salt, dummy));
}

#endif

BOOST_AUTO_TEST_CASE(RarReaderRar3SaltFieldTest)
{
	const uint8 salt[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	MemoryRarSource source(BuildRar3SaltVolume(Bytes(48, 5), "movie.mkv", salt, 8));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	BOOST_REQUIRE(!volume.GetFiles()->empty());
	RarFile& inner = volume.GetFiles()->front();
	BOOST_CHECK(inner.GetHasSalt());
	BOOST_CHECK(inner.GetEncryptedData());
	BOOST_CHECK_EQUAL_COLLECTIONS(inner.GetSalt(), inner.GetSalt() + 8, salt, salt + 8);
}

BOOST_AUTO_TEST_CASE(RarReaderRar3TruncatedSaltTest)
{
	// SALT flag set, HEAD_SIZE claims 8 salt bytes, but only 3 exist and nothing
	// follows: the salt read runs off the source end and the parse fails closed.
	const uint8 salt[3] = {0x11, 0x22, 0x33};
	MemoryRarSource source(BuildRar3SaltVolume(Bytes(0, 0), "movie.mkv", salt, 3, false));
	RarVolume volume("memory");
	BOOST_CHECK(!volume.ReadFrom(source));
}

BOOST_AUTO_TEST_CASE(RarReaderRar5CryptFieldTest)
{
	const uint8 salt[16] = {
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
	const uint8 iv[16] = {
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf};
	const uint8 check[12] = {
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb};

	Rar5CryptSpec spec;
	spec.KdfCount = 15;
	MemoryRarSource source(BuildRar5CryptVolume(Bytes(48, 7), "movie.mkv", spec, salt, iv, check));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	BOOST_REQUIRE(!volume.GetFiles()->empty());
	RarFile& inner = volume.GetFiles()->front();
	BOOST_CHECK(inner.GetEncryptedData());
	const RarFile::Rar5Crypt* crypt = inner.GetCrypt();
	BOOST_REQUIRE(crypt != nullptr);
	BOOST_CHECK_EQUAL(crypt->Version, 0u);
	BOOST_CHECK_EQUAL(crypt->Flags, 0x01u);
	BOOST_CHECK_EQUAL((int)crypt->KdfCount, 15);
	BOOST_CHECK(crypt->KdfCount <= 24);
	BOOST_CHECK(crypt->HasCheck);
	BOOST_CHECK_EQUAL_COLLECTIONS(crypt->Salt, crypt->Salt + 16, salt, salt + 16);
	BOOST_CHECK_EQUAL_COLLECTIONS(crypt->Iv, crypt->Iv + 16, iv, iv + 16);
	BOOST_CHECK_EQUAL_COLLECTIONS(crypt->CheckValue, crypt->CheckValue + 12, check, check + 12);
}

BOOST_AUTO_TEST_CASE(RarReaderRar5CryptNoCheckTest)
{
	// flags=0 => no password-check value; HasCheck must be false and the record
	// is 12 bytes shorter
	const uint8 salt[16] = {1};
	const uint8 iv[16] = {2};
	Rar5CryptSpec spec;
	spec.Flags = 0;
	spec.IncludeCheck = false;
	MemoryRarSource source(BuildRar5CryptVolume(Bytes(32, 9), "movie.mkv", spec, salt, iv, nullptr));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	const RarFile::Rar5Crypt* crypt = volume.GetFiles()->front().GetCrypt();
	BOOST_REQUIRE(crypt != nullptr);
	BOOST_CHECK(!crypt->HasCheck);
	BOOST_CHECK_EQUAL(crypt->Flags, 0u);
}

BOOST_AUTO_TEST_CASE(RarReaderRar5ShortCryptRecordTest)
{
	// an honestly-framed record that is too short for a full crypt payload
	// (salt only, no IV/check): the bounded parse ignores it - GetCrypt stays
	// null - and the rest of the volume still parses. No over-read.
	const uint8 salt[16] = {3};
	const uint8 iv[16] = {4};
	Rar5CryptSpec spec;
	spec.IncludeIv = false;
	spec.IncludeCheck = false;
	MemoryRarSource source(BuildRar5CryptVolume(Bytes(16, 11), "movie.mkv", spec, salt, iv, nullptr));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	BOOST_REQUIRE(!volume.GetFiles()->empty());
	RarFile& inner = volume.GetFiles()->front();
	BOOST_CHECK(inner.GetEncryptedData());		// the crypt record type still marks it
	BOOST_CHECK(inner.GetCrypt() == nullptr);	// but too short to expose parameters
}

BOOST_AUTO_TEST_CASE(RarReaderRar5TruncatedCryptRecordTest)
{
	// the record claims a full crypt body but the volume is cut mid-salt: the
	// salt read runs off the source end and the parse fails closed, never
	// reading past the buffer.
	const uint8 salt[16] = {5};
	const uint8 iv[16] = {6};
	const uint8 check[12] = {7};
	Rar5CryptSpec spec;
	spec.Trailing = false;
	size_t saltOffset = 0;
	std::vector<char> bytes =
		BuildRar5CryptVolume(Bytes(0, 0), "movie.mkv", spec, salt, iv, check, &saltOffset);
	BOOST_REQUIRE(saltOffset + 3 < bytes.size());
	bytes.resize(saltOffset + 3);	// keep 3 salt bytes, drop the rest
	MemoryRarSource source(std::move(bytes));
	RarVolume volume("memory");
	BOOST_CHECK(!volume.ReadFrom(source));
}

BOOST_AUTO_TEST_CASE(RarReaderRar5OversizeKdfCountTest)
{
	// a well-formed record with an absurd kdfCount parses without over-reading;
	// the exposed field is faithful, and the KDF layer is what refuses it.
	const uint8 salt[16] = {8};
	const uint8 iv[16] = {9};
	const uint8 check[12] = {10};
	Rar5CryptSpec spec;
	spec.KdfCount = 200;
	MemoryRarSource source(BuildRar5CryptVolume(Bytes(32, 13), "movie.mkv", spec, salt, iv, check));
	RarVolume volume("memory");
	BOOST_REQUIRE(volume.ReadFrom(source));
	const RarFile::Rar5Crypt* crypt = volume.GetFiles()->front().GetCrypt();
	BOOST_REQUIRE(crypt != nullptr);
	BOOST_CHECK_EQUAL((int)crypt->KdfCount, 200);
#ifndef DISABLE_TLS
	uint8 key[32] = {};
	BOOST_CHECK(!StreamCrypto::DeriveRar5("123", crypt->KdfCount, crypt->Salt, key));
#endif
}

BOOST_AUTO_TEST_SUITE_END()
