/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
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

#include <boost/test/unit_test.hpp>
#include "Decoder.h"
#include "YEncoder.h"
#include "YEncDecryptor.h"
#include "YEncKeyCache.h"
#include "YEncTestHelpers.h"
#include <sodium.h>
#include <iomanip>
#include <sstream>

// Shared test helpers : one hex/yEnc helper set for all NNTP test TUs.
using namespace YEncTest;

BOOST_AUTO_TEST_SUITE(NNTPTest)

std::string yEncEncode(const std::string& data)
{
	return YEncTest::SimpleYencEncode(data);
}

/**
 * Single message:
 * 
 * =ybegin line=128 size=111401 name=al_larsonbw030_ball.jpg\r\n
 * )_)=J*:tpsp*++++V+V**)_*m*0./0/.00/011024:44334>896:A>....\r\n
 * ...Â´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÌ´RÍ©)_\r\n
 * =yend size=111401\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(SingleMessageTest)
{
	Decoder decoder;
	decoder.SetCrcCheck(true);
	std::stringstream ss;

	const std::string data = "nzbget";
	const std::string crc = "a30bff0d";
	const std::string filename = "name.dat";
	const std::string encodedData = yEncEncode(data);

	ss << "=ybegin line=128 size=" << data.size() << " name=" << filename << "\r\n";
	ss << encodedData << "\r\n";
	ss << "=yend size=" << data.size() << " crc32=" << crc << "\r\n";
	ss << ".\r\n";

	std::string msg = ss.str();

	int len = decoder.DecodeBuffer(msg.data(), msg.size());

	auto status = decoder.Check();
	auto size = decoder.GetSize();
	auto calculatedCrc = decoder.GetCalculatedCrc();
	auto articleName = decoder.GetArticleFilename();
	auto format = decoder.GetFormat();
	auto eof = decoder.GetEof();
	auto res = msg.substr(0, len);

	BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
	BOOST_CHECK_EQUAL(articleName, filename);
	BOOST_CHECK_EQUAL(size, 6);
	BOOST_CHECK_EQUAL(calculatedCrc, 0xa30bff0d);
	BOOST_CHECK_EQUAL(format, Decoder::efYenc);
	BOOST_CHECK_EQUAL(eof, true);
	BOOST_CHECK_EQUAL(res, "nzbget");
}

/**
 * Multipart message:
 * 
 * =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=1 end=100000\r\n
 * .... data\r\n
 * =yend size=100000 part=1 pcrc32=abcdef12\r\n
 * .\r\n
 * =ybegin part=5 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=400001 end=500000\r\n
 * .... data\r\n
 * =yend size=100000 part=10 pcrc32=12a45c78 crc32=abcdef12\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(MultipartMessageTest)
{
	Decoder decoder;
	decoder.SetCrcCheck(true);
	std::stringstream ss;

	const std::string crc = "a30bff0d";
	const std::string data = "nzb";
	const std::string encodedData = yEncEncode(data);
	const std::string pcrc = "cb64ae30";
	const std::string filename = "name.dat";

	const std::string data2 = "get";
	const std::string encodedData2 = yEncEncode(data2);
	const std::string pcrc2 = "fd3b2e70";
	const size_t totalSize = data.size() + data2.size();

	ss << "=ybegin part=1 total=2 line=128 size=" << totalSize << " name=" << filename << "\r\n";
	ss << "=ypart begin=1 end=" << data.size() << "\r\n";
	ss << encodedData << "\r\n";
	ss << "=yend size=" << data.size() << " pcrc32=" << pcrc << "\r\n";
	ss << ".\r\n";

	std::string msg = ss.str();
	ss.str("");

	ss << "=ybegin part=2 total=2 line=128 size=" << totalSize << " name=" << filename << "\r\n";
	ss << "=ypart begin=" << data.size() + 1 << " end=" << totalSize << "\r\n";
	ss << encodedData2 << "\r\n";
	ss << "=yend size=" << data2.size() << " pcrc32=" << pcrc2 << " crc32=" << crc << "\r\n";
	ss << ".\r\n";

	std::string msg2 = ss.str();

	int len = decoder.DecodeBuffer(msg.data(), msg.size());
	{
		auto status = decoder.Check();
		auto size = decoder.GetSize();
		auto calculatedCrc = decoder.GetCalculatedCrc();
		auto articleName = decoder.GetArticleFilename();
		auto format = decoder.GetFormat();
		auto eof = decoder.GetEof();
		auto res = msg.substr(0, len);

		BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
		BOOST_CHECK_EQUAL(articleName, filename);
		BOOST_CHECK_EQUAL(size, totalSize);
		BOOST_CHECK_EQUAL(calculatedCrc, 0xcb64ae30);
		BOOST_CHECK_EQUAL(format, Decoder::efYenc);
		BOOST_CHECK_EQUAL(eof, true);
		BOOST_CHECK_EQUAL(res, "nzb");
	}

	decoder.Clear();
	decoder.SetCrcCheck(true);

	int len2 = decoder.DecodeBuffer(msg2.data(), msg2.size());

	auto articleName = decoder.GetArticleFilename();
	auto status = decoder.Check();
	auto size = decoder.GetSize();
	auto calculatedCrc = decoder.GetCalculatedCrc();
	auto eof = decoder.GetEof();
	auto res = msg2.substr(0, len);

	BOOST_CHECK_EQUAL(articleName, filename);
	BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
	BOOST_CHECK_EQUAL(len, data2.size());
	BOOST_CHECK_EQUAL(len + len2, totalSize);
	BOOST_CHECK_EQUAL(size, totalSize);
	BOOST_CHECK_EQUAL(calculatedCrc, 0xfd3b2e70);
	BOOST_CHECK_EQUAL(eof, true);
	BOOST_CHECK_EQUAL(res, "get");
}

/**
 * Unusual multipart message where there's no \r\n separating between name=... and =ypart:
 * 
 * =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat=ypart begin=1 end=100000\r\n
 * .... data\r\n
 * =yend size=100000 part=1 pcrc32=abcdef12\r\n
 * .\r\n
 * =ybegin part=5 line=128 size=500000 name=mybinary.dat\r\n
 * =ypart begin=400001 end=500000\r\n
 * .... data\r\n
 * =yend size=100000 part=10 pcrc32=12a45c78 crc32=abcdef12\r\n
 * .\r\n
*/
BOOST_AUTO_TEST_CASE(UnusualMultipartMessageTest)
{
	Decoder decoder;
	decoder.SetCrcCheck(true);
	std::stringstream ss;

	const std::string crc = "a30bff0d";
	const std::string data = "nzb";
	const std::string encodedData = yEncEncode(data);
	const std::string pcrc = "cb64ae30";
	const std::string filename = "name.dat";

	const std::string data2 = "get";
	const std::string encodedData2 = yEncEncode(data2);
	const std::string pcrc2 = "fd3b2e70";
	const size_t totalSize = data.size() + data2.size();

	ss << "=ybegin part=1 total=2 line=128 size=" << totalSize << " name=" << filename;
	ss << "=ypart begin=1 end=" << data.size() << "\r\n";
	ss << encodedData << "\r\n";
	ss << "=yend size=" << data.size() << " pcrc32=" << pcrc << "\r\n";
	ss << ".\r\n";

	std::string msg = ss.str();
	ss.str("");

	ss << "=ybegin part=2 total=2 line=128 size=" << totalSize << " name=" << filename;
	ss << "=ypart begin=" << data.size() + 1 << " end=" << totalSize << "\r\n";
	ss << encodedData2 << "\r\n";
	ss << "=yend size=" << data2.size() << " pcrc32=" << pcrc2 << " crc32=" << crc << "\r\n";
	ss << ".\r\n";

	std::string msg2 = ss.str();

	int len = decoder.DecodeBuffer(msg.data(), msg.size());
	{
		auto status = decoder.Check();
		auto size = decoder.GetSize();
		auto calculatedCrc = decoder.GetCalculatedCrc();
		auto articleName = decoder.GetArticleFilename();
		auto format = decoder.GetFormat();
		auto eof = decoder.GetEof();
		auto res = msg.substr(0, len);

		BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
		BOOST_CHECK_EQUAL(articleName, filename);
		BOOST_CHECK_EQUAL(size, totalSize);
		BOOST_CHECK_EQUAL(calculatedCrc, 0xcb64ae30);
		BOOST_CHECK_EQUAL(format, Decoder::efYenc);
		BOOST_CHECK_EQUAL(eof, true);
		BOOST_CHECK_EQUAL(res, "nzb");
	}

	decoder.Clear();
	decoder.SetCrcCheck(true);

	int len2 = decoder.DecodeBuffer(msg2.data(), msg2.size());

	auto articleName = decoder.GetArticleFilename();
	auto status = decoder.Check();
	auto size = decoder.GetSize();
	auto calculatedCrc = decoder.GetCalculatedCrc();
	auto eof = decoder.GetEof();
	auto res = msg2.substr(0, len);

	BOOST_CHECK_EQUAL(articleName, filename);
	BOOST_CHECK_EQUAL(status, Decoder::dsFinished);
	BOOST_CHECK_EQUAL(len, data2.size());
	BOOST_CHECK_EQUAL(len + len2, totalSize);
	BOOST_CHECK_EQUAL(size, totalSize);
	BOOST_CHECK_EQUAL(calculatedCrc, 0xfd3b2e70);
	BOOST_CHECK_EQUAL(eof, true);
	BOOST_CHECK_EQUAL(res, "get");
}

BOOST_AUTO_TEST_CASE(BufferOverflowTest)
{
	Decoder decoder;
	decoder.SetRawMode(true);

	constexpr int bufSize = 4096;
	auto buf = std::make_unique<char[]>(bufSize);
	memset(buf.get(), 'x', bufSize);

	int len = decoder.DecodeBuffer(buf.get(), bufSize);
	BOOST_CHECK_EQUAL(len, bufSize);
	BOOST_CHECK_EQUAL(decoder.GetEof(), false);
}

BOOST_AUTO_TEST_CASE(EncryptedWireSinglePartTest)
{
	// Built with body-vec-01 parameters (password "conformance-fixture-password", salt
	// "1a2b3c4d5e6f7890abcdef1234567890", segmentIndex 1, AAD name=test.dat,
	// partNumber=1, partBegin=1, segmentBytes=16, 16-byte plaintext
	// "Hello World.txt\xff") and encrypted in-test; the vector's
	// ciphertext/tag bytes are not used.
	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> expectedPlaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	uint32_t segmentIndex = 1;

	// A valid 16-byte salt may itself begin with "=ybegin " or "begin "
	// (the 253-byte salt Alphabet allows it). Each salt must round-trip.
	std::vector<std::vector<uint8_t>> salts = {
		YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890"),
		{'=', 'y', 'b', 'e', 'g', 'i', 'n', ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'},
		{'b', 'e', 'g', 'i', 'n', ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
	};

	for (const auto& salt : salts)
	{
		for (bool chunked : {true, false})
		{
			BOOST_TEST_MESSAGE("combo salt=" + YEncTest::BinToHex(salt) + " chunked=" << (chunked ? 1 : 0));
			// Build raw wire article:
			// line 1: =ybegin line=128 size=16 name=test.dat
			// line 2: =yencryption cipher=XChaCha20-Poly1305 salt=<hex> index=00000001 tag=<hex>
			// line 3: <yenc encoded ct>
			// line 4: =yend size=16 crc32=...
			// line 5: .
			// The body ciphertext/tag depend on the salt through the Argon2id
			// master key and the derived nonce, so encrypt the body per salt
			// with the same derivations the decoder uses.
			YEncDecryptor encDec(password);
			BOOST_REQUIRE(encDec.EnsureMasterKey(salt.data()));
			uint8_t nonce[24];
			BOOST_REQUIRE(encDec.DeriveBodyNonce(segmentIndex, nonce));
			std::vector<uint8_t> aad = YEncDecryptor::BuildAad("test.dat", 1, 1, 16);
			BOOST_REQUIRE(!aad.empty());
			std::vector<uint8_t> ct(expectedPlaintext.size());
			std::vector<uint8_t> tag(crypto_aead_xchacha20poly1305_ietf_ABYTES);
			crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ct.data(), tag.data(), nullptr,
				expectedPlaintext.data(), expectedPlaintext.size(), aad.data(), aad.size(),
				nullptr, nonce, encDec.GetCachedMasterKey().data());

			std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
			std::vector<uint8_t> wireLine1;
			BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
				reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
				segmentIndex, 1, true, salt.data(), wireLine1
			)), static_cast<int>(YEncDecryptor::Status::Ok));

			std::ostringstream encOss;
			encOss << "=yencryption cipher=XChaCha20-Poly1305 salt=" << YEncTest::BinToHex(salt)
				<< " index=00000001 tag=" << YEncTest::BinToHex(tag);
			std::string line2Plain = encOss.str();
			std::vector<uint8_t> wireLine2;
			BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
				reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
				segmentIndex, 2, false, salt.data(), wireLine2
			)), static_cast<int>(YEncDecryptor::Status::Ok));

			std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

			// Calculate ciphertext CRC32
			Crc32 crc;
			crc.Append(ct.data(), ct.size());
			uint32_t ctCrc = crc.Finish();
			std::ostringstream endOss;
			endOss << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc;
			std::string line4Plain = endOss.str();

			std::vector<uint8_t> wireLine4;
			BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
				reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
				segmentIndex, 4, false, salt.data(), wireLine4
			)), static_cast<int>(YEncDecryptor::Status::Ok));

			std::string fullWire;
			fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
			fullWire.append("\r\n");
			fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
			fullWire.append("\r\n");
			fullWire.append(wireLine3);
			fullWire.append("\r\n");
			fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
			fullWire.append("\r\n.\r\n");

			Decoder decoder;
			decoder.SetDecryptor(&encDec);
			decoder.SetCrcCheck(true);

			size_t chunkSize = chunked ? 32 : fullWire.size();
			for (size_t offset = 0; offset < fullWire.size(); offset += chunkSize)
			{
				size_t n = std::min(chunkSize, fullWire.size() - offset);
				std::string chunk = fullWire.substr(offset, n);
				decoder.DecodeBuffer(chunk.data(), static_cast<int>(chunk.size()));
			}

			auto status = decoder.Check();
			BOOST_CHECK_EQUAL(static_cast<int>(status), static_cast<int>(Decoder::dsFinished));
			BOOST_CHECK_EQUAL(decoder.GetArticleFilename(), "test.dat");
			BOOST_CHECK(decoder.IsEncrypted());
			BOOST_CHECK_EQUAL(decoder.GetExpectedCrc(), ctCrc);
			BOOST_CHECK_EQUAL(decoder.GetCalculatedCrc(), ctCrc);

			const auto& pt = decoder.GetDecryptedData();
			BOOST_CHECK_EQUAL(pt.size(), expectedPlaintext.size());
			BOOST_CHECK_EQUAL_COLLECTIONS(pt.begin(), pt.end(), expectedPlaintext.begin(), expectedPlaintext.end());
		}
	}
}

BOOST_AUTO_TEST_CASE(EncryptedWireMultipartTest)
{
	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	std::vector<uint8_t> tag = YEncTest::HexToBin("bc88aefe81a81300125666343210e667");
	std::vector<uint8_t> expectedPlaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	uint32_t segmentIndex = 1;

	// Multipart lines:
	// line 1: =ybegin part=1 total=2 line=128 size=32 name=multi.dat
	// line 2: =ypart begin=1 end=16
	// line 3: =yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=bc88aefe81a81300125666343210e667
	// line 4: <yenc encoded ct>
	// line 5: =yend size=16 part=1 pcrc32=...
	// line 6: .
	YEncDecryptor encDec(password);
	std::string line1Plain = "=ybegin part=1 total=2 line=128 size=32 name=multi.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = "=ypart begin=1 end=16";
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line3Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=bc88aefe81a81300125666343210e667";
	std::vector<uint8_t> wireLine3;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line3Plain.data()), line3Plain.size(),
		segmentIndex, 3, false, salt.data(), wireLine3
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wireLine4 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

	Crc32 crc;
	crc.Append(ct.data(), ct.size());
	uint32_t ctCrc = crc.Finish();
	std::ostringstream endOss;
	endOss << "=yend size=16 part=1 pcrc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc;
	std::string line5Plain = endOss.str();

	std::vector<uint8_t> wireLine5;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line5Plain.data()), line5Plain.size(),
		segmentIndex, 5, false, salt.data(), wireLine5
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string fullWire;
	fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine3.data()), wireLine3.size());
	fullWire.append("\r\n");
	fullWire.append(wireLine4);
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine5.data()), wireLine5.size());
	fullWire.append("\r\n.\r\n");

	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.SetCrcCheck(true);

	size_t chunkSize = 25;
	for (size_t offset = 0; offset < fullWire.size(); offset += chunkSize)
	{
		size_t n = std::min(chunkSize, fullWire.size() - offset);
		std::string chunk = fullWire.substr(offset, n);
		decoder.DecodeBuffer(chunk.data(), static_cast<int>(chunk.size()));
	}

	auto status = decoder.Check();
	BOOST_CHECK_EQUAL(static_cast<int>(status), static_cast<int>(Decoder::dsFinished));
	BOOST_CHECK_EQUAL(decoder.GetArticleFilename(), "multi.dat");
	BOOST_CHECK_EQUAL(decoder.GetBeginPos(), 1);
	BOOST_CHECK_EQUAL(decoder.GetEndPos(), 16);
	BOOST_CHECK(decoder.IsEncrypted());
	BOOST_CHECK_EQUAL(decoder.GetExpectedCrc(), ctCrc);
	BOOST_CHECK_EQUAL(decoder.GetCalculatedCrc(), ctCrc);

	const auto& pt = decoder.GetDecryptedData();
	BOOST_CHECK_EQUAL(pt.size(), expectedPlaintext.size());
	BOOST_CHECK_EQUAL_COLLECTIONS(pt.begin(), pt.end(), expectedPlaintext.begin(), expectedPlaintext.end());
}

BOOST_AUTO_TEST_CASE(DecoderDualBootstrapMismatchFailsClosedTest)
{
	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> salt1 = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::vector<uint8_t> salt2 = YEncTest::HexToBin("ffffffffffffffffffffffffffffffff");
	std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	uint32_t segmentIndex = 1;

	YEncDecryptor encDec(password);
	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt1.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	// Header has mismatched salt (salt2 instead of salt1)
	std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=ffffffffffffffffffffffffffffffff index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e";
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt1.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

	Crc32 crc;
	crc.Append(ct.data(), ct.size());
	uint32_t ctCrc = crc.Finish();
	std::ostringstream endOss;
	endOss << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc;
	std::string line4Plain = endOss.str();

	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
		segmentIndex, 4, false, salt1.data(), wireLine4
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string fullWire;
	fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
	fullWire.append("\r\n");
	fullWire.append(wireLine3);
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
	fullWire.append("\r\n.\r\n");

	// Test Decoder discovers from wire without SetSegmentIndex, but fails closed on Dual-Bootstrap mismatch
	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.SetCrcCheck(true);

	decoder.DecodeBuffer(fullWire.data(), static_cast<int>(fullWire.size()));

	auto checkStatus = decoder.Check();
	BOOST_CHECK(checkStatus == Decoder::dsAuthFailed);
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

BOOST_AUTO_TEST_CASE(EncryptedWireBufferOverflowTest)
{
	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> salt1 = YEncTest::HexToBin("0123456789abcdef0123456789abcdef");
	uint32_t segmentIndex = 1;

	YEncDecryptor encDec(password);
	std::string line1Plain = "=ybegin line=128 size=1024 name=large.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt1.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line1Wire(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	line1Wire += "\r\n";

	Decoder decoder;
	decoder.SetDecryptor(&encDec);

	// Feed line 1 to activate encrypted wire mode
	decoder.DecodeBuffer(line1Wire.data(), static_cast<int>(line1Wire.size()));

	// Now feed data chunks exceeding the 16MB ceiling (17 MB total) without terminating dot
	std::vector<char> largeChunk(1024 * 1024, 'A');
	for (int i = 0; i < 17; ++i)
	{
		decoder.DecodeBuffer(largeChunk.data(), static_cast<int>(largeChunk.size()));
	}

	// Must fail closed on buffer ceiling overflow. Exceeding the local
	// 16 MiB article bound is a local failure (NG12-2), so the status is
	// dsFatalError rather than provider-corruption dsAuthFailed.
	auto status = decoder.Check();
	BOOST_CHECK_EQUAL(status, Decoder::dsFatalError);
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

BOOST_AUTO_TEST_CASE(DecoderAuthFailedCheckOrderTest)
{
	// 1. Incomplete article: without auth failure Check() returns dsArticleIncomplete;
	// with m_authFailed set, dsAuthFailed preempts dsArticleIncomplete.
	{
		Decoder decoder;
		std::string incompleteMsg = "=ybegin line=128 size=16 name=test.dat\r\nsomedata\r\n";
		decoder.DecodeBuffer(incompleteMsg.data(), static_cast<int>(incompleteMsg.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsArticleIncomplete);

		decoder.SetAuthFailed(true);
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
	}

	// 2. Size mismatch: without auth failure Check() returns dsInvalidSize;
	// with m_authFailed set, dsAuthFailed preempts dsInvalidSize.
	{
		Decoder decoder;
		std::string sizeMismatchMsg =
			"=ybegin line=128 size=16 name=test.dat\r\n"
			"shortdata\r\n"
			"=yend size=16\r\n"
			".\r\n";
		decoder.DecodeBuffer(sizeMismatchMsg.data(), static_cast<int>(sizeMismatchMsg.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsInvalidSize);

		decoder.SetAuthFailed(true);
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
	}

	// 3. Real wire path: an auth-failing encrypted article (valid tag and
	// ciphertext, corrupted wire CRC in =yend, as in
	// EncryptedWireBadCrcFailClosedTest) must surface dsAuthFailed through
	// the genuine decrypt path, preempting the CRC/size status the
	// competing branches would otherwise report.
	{
		std::string password = "conformance-fixture-password";
		std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
		std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
		uint32_t segmentIndex = 1;

		YEncDecryptor encDec(password);
		std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
		std::vector<uint8_t> wireLine1;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
			segmentIndex, 1, true, salt.data(), wireLine1
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e";
		std::vector<uint8_t> wireLine2;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
			segmentIndex, 2, false, salt.data(), wireLine2
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

		// Corrupted wire CRC: without the auth-failure precedence this
		// article would report a CRC/size-class status instead.
		std::string line4Plain = "=yend size=16 crc32=deadbeef";
		std::vector<uint8_t> wireLine4;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
			segmentIndex, 4, false, salt.data(), wireLine4
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string fullWire;
		fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		fullWire.append("\r\n");
		fullWire.append(wireLine3);
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
		fullWire.append("\r\n.\r\n");

		Decoder decoder;
		decoder.SetDecryptor(&encDec);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(fullWire.data(), static_cast<int>(fullWire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

BOOST_AUTO_TEST_CASE(DecoderSegmentIndexResetsBetweenArticlesTest)
{
	// The decoder's bootstrap-extracted index must reset between articles —
	// a stale index from a previous decode would corrupt dual-bootstrap
	// agreement for the next article.
	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	const std::vector<uint8_t> expectedPlaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");

	auto buildWire = [&](uint32_t segmentIndex) -> std::string
	{
		YEncDecryptor encDec(password);
		std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
		std::vector<uint8_t> wireLine1;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
			segmentIndex, 1, true, salt.data(), wireLine1
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		char indexHex[9];
		snprintf(indexHex, sizeof(indexHex), "%08x", segmentIndex);
		std::string line2Plain = std::string(
			"=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=") +
			indexHex + " tag=da7a9fa260fa4d336ae8d139864d320e";
		std::vector<uint8_t> wireLine2;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
			segmentIndex, 2, false, salt.data(), wireLine2
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));
		Crc32 wireCrc; wireCrc.Append(const_cast<uint8_t*>(ct.data()), ct.size());
	std::ostringstream crcFooter;
	crcFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << wireCrc.Finish();
	std::string line4Plain = crcFooter.str();
		std::vector<uint8_t> wireLine4;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
			segmentIndex, 4, false, salt.data(), wireLine4
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string fullWire;
		fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		fullWire.append("\r\n");
		fullWire.append(wireLine3);
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
		fullWire.append("\r\n.\r\n");
		return fullWire;
	};

	YEncDecryptor decryptor(password);
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);

	// Article 1: index 1
	{
		std::string wire = buildWire(1);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL(decoder.GetSegmentIndex(), 1U);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}

	// Article 2 on the SAME decoder: index 2 — Clear() (as ArticleDownloader
	// does per article) must reset the bootstrap-extracted index so the stale
	// index 1 cannot leak into dual-bootstrap agreement.
	decoder.Clear();
	{
		std::string wire = buildWire(2);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		// The body ciphertext/tag were produced under index 1's nonce. With a
		// correctly reset index (2) authentication MUST fail; a stale index 1
		// would falsely authenticate the article (regression guard).
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK_EQUAL(decoder.GetSegmentIndex(), 2U);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

BOOST_AUTO_TEST_CASE(DotUnstuffingBeforeBootstrapExtractionTest)
{
	// Control Std v1.2 §9 "Transport boundary": a bootstrap Line 1
	// starting with '.' is dot-stuffed on the wire; consumers MUST unstuff
	// before line splitting/bootstrap extraction.
	const std::string password = "conformance-fixture-password";
	// Body tag/ciphertext from body_encryption.json vector body-vec-01 (index 1 nonce);
	// decrypted body yields expectedPlaintext.
	const uint32_t segmentIndex = 1;
	const std::vector<uint8_t> expectedPlaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	YEncDecryptor encDec(password);

	// Force a Line 1 whose first wire byte is '.' (0x2E) via Line 1 plaintext
	// beginning with '=ybegin' won't work; instead verify unstuffing of a data
	// line beginning with '.' inside the encrypted block, and verify a stuffed
	// terminator is not misread.
	std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = std::string(
		"=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e");
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	// Data line: on the wire a line whose content starts with '.' MUST be
	// dot-stuffed by the producer (RFC 3977 §3.1.1). The consumer's unstuffing
	// owner must strip the escape dot before yEnc decode / bootstrap extraction.
	std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));
	BOOST_REQUIRE_EQUAL((int)(uint8_t)wireLine3[0], 0x3b); // canonical vector body line

	Crc32 wireCrc; wireCrc.Append(const_cast<uint8_t*>(ct.data()), ct.size());
	std::ostringstream crcFooter;
	crcFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << wireCrc.Finish();
	std::string line4Plain = crcFooter.str();
	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
		segmentIndex, 4, false, salt.data(), wireLine4
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	auto assemble = [&](const std::string& bodyLine) -> std::string
	{
		std::string fullWire;
		fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		fullWire.append("\r\n");
		fullWire.append(bodyLine);
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
		fullWire.append("\r\n.\r\n");
		return fullWire;
	};

	Decoder decoder;
	decoder.SetDecryptor(&encDec);

	// Already-unstuffed input decodes unchanged (single unstuffing owner).
	std::string wire = assemble(wireLine3);
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
	BOOST_CHECK_EQUAL(decoder.GetSegmentIndex(), segmentIndex);
	BOOST_CHECK_EQUAL_COLLECTIONS(
		decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
		expectedPlaintext.begin(), expectedPlaintext.end());

	// Genuine stuffed sub-case: a data line whose yEnc wire form begins
	// with '.' is sent as '..' (RFC 3977 §3.1.1); the single unstuffing
	// pass must strip the escape dot so the identical plaintext is
	// recovered (same expectation as the unstuffed decode above, via a
	// ciphertext whose first byte yEnc-encodes to '.').
	{
		BOOST_REQUIRE(encDec.EnsureMasterKey(salt.data()));
		uint8_t nonce[24];
		BOOST_REQUIRE(encDec.DeriveBodyNonce(segmentIndex, nonce));
		const std::vector<uint8_t> aad = YEncDecryptor::BuildAad("test.dat", 1, 1, 16);

		// Brute-force a 16-byte plaintext whose ciphertext starts with
		// byte 0x04 (yEnc wire '.') and whose second byte does not need
		// '=' escaping, mirroring EncryptedWireRestoredBlockLeadingDotTest.
		std::vector<uint8_t> dotPt(16, 0), dotCt, dotTag;
		bool found = false;
		for (unsigned a = 0; a < 256 && !found; ++a)
		{
			dotPt[0] = static_cast<uint8_t>(a);
			for (unsigned b = 0; b < 256; ++b)
			{
				dotPt[1] = static_cast<uint8_t>(b);
				std::vector<uint8_t> ctTry(dotPt.size());
				std::vector<uint8_t> tagTry(crypto_aead_xchacha20poly1305_ietf_ABYTES);
				crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ctTry.data(), tagTry.data(), nullptr,
					dotPt.data(), dotPt.size(), aad.data(), aad.size(),
					nullptr, nonce, encDec.GetCachedMasterKey().data());
				if (ctTry[0] == 0x04 && ctTry[1] != 0x13)
				{
					dotCt = ctTry;
					dotTag = tagTry;
					found = true;
					break;
				}
			}
		}
		BOOST_REQUIRE(found);

		// Full yEnc escaping for the data line (the simple helper does not
		// escape '='); the resulting line must begin with '.'.
		std::string dotLine;
		for (uint8_t c : dotCt)
		{
			uint8_t e = static_cast<uint8_t>((c + 42) % 256);
			if (e == 0x00 || e == 0x0A || e == 0x0D || e == 0x3D)
			{
				dotLine += '=';
				e = static_cast<uint8_t>((e + 64) % 256);
			}
			dotLine += static_cast<char>(e);
		}
		BOOST_REQUIRE_EQUAL(static_cast<int>(static_cast<uint8_t>(dotLine[0])), 0x2E); // '.'
		const std::string stuffedLine = "." + dotLine;

		std::string dotLine2Plain = std::string(
			"=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=") +
			YEncTest::BinToHex(dotTag);
		std::vector<uint8_t> dotWireLine2;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(dotLine2Plain.data()), dotLine2Plain.size(),
			segmentIndex, 2, false, salt.data(), dotWireLine2
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		Crc32 dotCrc; dotCrc.Append(const_cast<uint8_t*>(dotCt.data()), dotCt.size());
		std::ostringstream dotFooter;
		dotFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << dotCrc.Finish();
		std::string dotLine4Plain = dotFooter.str();
		std::vector<uint8_t> dotWireLine4;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(dotLine4Plain.data()), dotLine4Plain.size(),
			segmentIndex, 4, false, salt.data(), dotWireLine4
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string stuffedWire;
		stuffedWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		stuffedWire.append("\r\n");
		stuffedWire.append(reinterpret_cast<const char*>(dotWireLine2.data()), dotWireLine2.size());
		stuffedWire.append("\r\n");
		stuffedWire.append(stuffedLine);
		stuffedWire.append("\r\n");
		stuffedWire.append(reinterpret_cast<const char*>(dotWireLine4.data()), dotWireLine4.size());
		stuffedWire.append("\r\n.\r\n");

		Decoder stuffedDecoder;
		stuffedDecoder.SetDecryptor(&encDec);
		stuffedDecoder.DecodeBuffer(stuffedWire.data(), static_cast<int>(stuffedWire.size()));
		BOOST_CHECK_EQUAL(stuffedDecoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL(stuffedDecoder.GetSegmentIndex(), segmentIndex);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			stuffedDecoder.GetDecryptedData().begin(), stuffedDecoder.GetDecryptedData().end(),
			dotPt.begin(), dotPt.end());
	}
}

BOOST_AUTO_TEST_CASE(DotUnstuffingChunkBoundaryTerminatorTest)
{
	// A chunk ending exactly after "\r\n." must not consume the
	// following article terminator ".\r\n" as unstuffing input — the
	// line-start '.' held back at the chunk boundary is the terminator's dot
	// (or the first half of a split stuffed pair), not an escape dot to strip.
	// Feed the same article in chunks split at every position around the
	// final CRLF boundary and require byte-identical results to the
	// single-chunk decode.
	const std::string password = "conformance-fixture-password";
	const uint32_t segmentIndex = 1;
	const std::vector<uint8_t> expectedPlaintext = YEncTest::HexToBin("48656c6c6f20576f726c642e747874ff");
	YEncDecryptor encDec(password);

	std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = std::string(
		"=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e");
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

	Crc32 wireCrc; wireCrc.Append(const_cast<uint8_t*>(ct.data()), ct.size());
	std::ostringstream crcFooter;
	crcFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << wireCrc.Finish();
	std::string line4Plain = crcFooter.str();
	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
		segmentIndex, 4, false, salt.data(), wireLine4
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string fullWire;
	fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
	fullWire.append("\r\n");
	fullWire.append(wireLine3);
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
	fullWire.append("\r\n.\r\n");

	auto decodeWhole = [&](Decoder& d)
	{
		d.SetDecryptor(&encDec);
		d.DecodeBuffer(fullWire.data(), static_cast<int>(fullWire.size()));
	};

	auto decodeChunked = [&](Decoder& d, size_t firstChunk)
	{
		d.SetDecryptor(&encDec);
		d.DecodeBuffer(fullWire.data(), static_cast<int>(firstChunk));
		d.DecodeBuffer(fullWire.data() + firstChunk,
			static_cast<int>(fullWire.size() - firstChunk));
	};

	// Reference: single-chunk decode.
	Decoder reference;
	decodeWhole(reference);
	BOOST_CHECK_EQUAL(reference.Check(), Decoder::dsFinished);
	BOOST_CHECK_EQUAL(reference.GetSegmentIndex(), segmentIndex);
	BOOST_CHECK_EQUAL_COLLECTIONS(
		reference.GetDecryptedData().begin(), reference.GetDecryptedData().end(),
		expectedPlaintext.begin(), expectedPlaintext.end());

	// Chunk splits at and around the "\r\n." / terminator boundary, including
	// the exact boundary cases: right after "\r\n." (before ".\r\n"), right
	// after "\r\n.\r", and right after a lone line-start ".".
	size_t terminatorPos = fullWire.size() - 5; // position of the final ".\r\n"
	std::vector<size_t> splits = {
		terminatorPos - 1,     // chunk ends after "\r\n" of =yend line
		terminatorPos,         // chunk ends exactly after "\r\n."
		terminatorPos + 1,     // chunk ends exactly after "\r\n.\r"
		terminatorPos + 2,     // chunk ends exactly after "\r\n.\r\n"
		terminatorPos - 4,     // chunk ends inside the stuffed/escape region
	};
	for (size_t split : splits)
	{
		Decoder chunked;
		decodeChunked(chunked, split);
		std::string label = "chunk split at byte " + std::to_string(split);
		BOOST_CHECK_MESSAGE(chunked.Check() == Decoder::dsFinished,
			("chunked decode did not finish: " + label).c_str());
		BOOST_CHECK_EQUAL_COLLECTIONS(
			chunked.GetDecryptedData().begin(), chunked.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}

	// Also feed byte-by-byte across the last 12 bytes to stress every
	// boundary alignment of the terminator.
	{
		Decoder chunked;
		chunked.SetDecryptor(&encDec);
		size_t tailStart = fullWire.size() - 12;
		chunked.DecodeBuffer(fullWire.data(), static_cast<int>(tailStart));
		for (size_t i = tailStart; i < fullWire.size(); ++i)
		{
			chunked.DecodeBuffer(fullWire.data() + i, 1);
		}
		BOOST_CHECK_EQUAL(chunked.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			chunked.GetDecryptedData().begin(), chunked.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}
}

// A stuffed data line ".." that ends a chunk becomes "." after unstuffing; it
// must not be taken for the article terminator.
BOOST_AUTO_TEST_CASE(StuffedDotLineAtChunkEndIsNotTerminatorTest)
{
	YEncDecryptor encDec("conformance-fixture-password");
	std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		1, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string chunk;
	chunk.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	chunk.append("\r\nline2\r\n..\r\n");

	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.DecodeBuffer(chunk.data(), static_cast<int>(chunk.size()));
	BOOST_CHECK(!decoder.GetEof());

	// The real terminator, split from its preceding CRLF, still ends the article.
	std::string terminator = ".\r\n";
	decoder.DecodeBuffer(terminator.data(), static_cast<int>(terminator.size()));
	BOOST_CHECK(decoder.GetEof());
}

// Plaintext framing on line 1 of an encrypted job fails before any key
// derivation (no Argon2id run, nothing cached).
BOOST_AUTO_TEST_CASE(PlaintextLine1RejectedBeforeKeyDerivationTest)
{
	// A Line 1 arriving as plaintext =ybegin/uue "begin" is not a valid
	// encrypted bootstrap (the 253-byte salt Alphabet allows salts that
	// themselves begin with "=ybegin " or "begin ", so no wire prefix
	// shortcut may reject before FF1): the bootstrap restoration must
	// fail closed with dsAuthFailed and no output. The Argon2id key
	// derivation may run before the FF1 failure is recognized.
	YEncKeyCache keyCache;
	YEncDecryptor encDec("conformance-fixture-password", &keyCache);
	for (const char* article : {
		"=ybegin line=128 size=4 name=test.dat\r\n=yend size=4\r\n.\r\n",
		"begin 644 test.dat\r\nend\r\n.\r\n" })
	{
		Decoder decoder;
		decoder.SetDecryptor(&encDec);
		std::string wire = article;
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK_EQUAL(decoder.GetDecryptedData().size(), 0u);
	}
}

BOOST_AUTO_TEST_CASE(OversizedLine1RejectedBeforeFf1IsFatalTest)
{
	// NG12-2: the oversized-Line-1 local limit is fatal (dsFatalError),
	// not provider corruption (dsAuthFailed).
	YEncDecryptor encDec("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	std::string wire = "1a2b3c4d5e6f7890abcdef123456789000000001";
	wire += std::string(YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE + 1 - 20, 'a');
	wire += "\r\n.\r\n";
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFatalError));
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

BOOST_AUTO_TEST_CASE(OversizedLine1RejectedBeforeFf1Test)
{
	// Control Std v1.2 §9 (Article-Bounded Memory): a receiver MAY reject an
	// article above its local limit before decryption. FF1 decryption is
	// quadratic in the line length (the round PRF converts the whole half to
	// a BIGNUM per round), so a hostile multi-megabyte Line 1 must be
	// rejected before it reaches FF1 instead of tying up a download thread.
	YEncKeyCache keyCache;
	YEncDecryptor encDec("conformance-fixture-password", &keyCache);
	Decoder decoder;
	decoder.SetDecryptor(&encDec);

	// 64 KiB + 1 of alphabet bytes (no 0x00/0x0A/0x0D) after the 20-byte
	// bootstrap: valid length/framing for the wire path but over the cap.
	std::string wire = "1a2b3c4d5e6f7890abcdef123456789000000001";
	wire += std::string(YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE + 1 - 20, 'a');
	wire += "\r\n.\r\n";
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	// NG12-2: the oversized-line local limit is fatal, not failover.
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFatalError));
	// Rejection must happen before any key derivation: no salt was cached.
	BOOST_CHECK_EQUAL(keyCache.Size(), 0u);

	// Sanity: a Line 1 just below the cap passes the cap check and derives
	// and caches the master key before failing later in FF1 (garbage
	// ciphertext) — proving the cap itself is not what failed the second
	// article.
	Decoder decoder2;
	decoder2.SetDecryptor(&encDec);
	std::string wire2 = "1a2b3c4d5e6f7890abcdef123456789000000001";
	wire2 += std::string(4096, 'a');
	wire2 += "\r\n.\r\n";
	decoder2.DecodeBuffer(wire2.data(), static_cast<int>(wire2.size()));
	BOOST_CHECK_EQUAL(static_cast<int>(decoder2.Check()), static_cast<int>(Decoder::dsAuthFailed));
	BOOST_CHECK_EQUAL(keyCache.Size(), 1u);
}

// NG12-1: a hostile block of nothing but separators (100k empty lines,
// ~100 KB, far under the 16 MiB cap) must fail closed quickly without
// materialising a line descriptor per physical line. Structurally the
// bound is enforced in RestoreControlLines (only lines 1-3 + footer are
// ever held); this case pins the fail-closed behaviour at that scale.
BOOST_AUTO_TEST_CASE(EncryptedSeparatorFloodFailsClosedTest)
{
	YEncDecryptor decryptor("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);
	std::string wire(100000, '\n');
	wire += ".\r\n";
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

// Control Std v1.2 §9 (Article-Bounded Memory): the cap bounds EVERY
// control-line decryption, not just Line 1 — RestoreControlLines also
// FF1-decrypts =ypart, =yencryption and the footer, and a hostile oversized
// non-Line-1 control line must be rejected before reaching FF1.
BOOST_AUTO_TEST_CASE(OversizedNonLine1ControlLineRejectedBeforeFf1Test)
{
	// Line 1 is a valid encrypted =ybegin; line 2 (the =yencryption control
	// line position) is oversized beyond the cap.
	const std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	YEncDecryptor encDec("conformance-fixture-password");
	const std::string line1Plain = "=ybegin line=128 size=16 name=file.bin";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		1, 1, true, salt.data(), wireLine1)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wire;
	wire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size()).append("\r\n");
	wire.append(YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE + 1, 'a').append("\r\n.\r\n");

	YEncKeyCache keyCache;
	YEncDecryptor encDec2("conformance-fixture-password", &keyCache);
	Decoder decoder;
	decoder.SetDecryptor(&encDec2);
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFatalError));
	// Line 1 decrypted fine (its key was derived and cached); the failure is
	// the oversized line 2, rejected by the cap before reaching FF1.
	BOOST_CHECK_EQUAL(keyCache.Size(), 1u);
}

// The restored block fed back through DecodeBuffer must not be
// dot-unstuffed a second time — a restored yEnc data line beginning with '.'
// (wire ct byte 0x04) or with '..' (ct bytes 0x04 0x04) must decode
// byte-identically with the Poly1305 tag still verifying.
BOOST_AUTO_TEST_CASE(EncryptedWireRestoredBlockLeadingDotTest)
{
	const std::string password = "conformance-fixture-password";
	const uint32_t segmentIndex = 1;
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	YEncDecryptor encDec(password);
	BOOST_REQUIRE(encDec.EnsureMasterKey(salt.data()));

	uint8_t nonce[24];
	BOOST_REQUIRE(encDec.DeriveBodyNonce(segmentIndex, nonce));

	// Brute-force a 16-byte plaintext whose XChaCha20-Poly1305 ciphertext
	// (first 16 bytes, tag excluded) starts with the yEnc wire byte(s) we
	// want on the restored data line: '.' => ct byte 0x04; '..' => 0x04 0x04.
	// The AEAD uses the Section 2.1 AAD (name=dot.dat, single part, 16 bytes)
	// exactly as the decoder computes it at authentication time.
	auto encryptPt = [&](const std::vector<uint8_t>& pt) -> std::vector<uint8_t>
	{
		std::vector<uint8_t> aad = YEncDecryptor::BuildAad("dot.dat", 1, 1, 16);
		std::vector<uint8_t> out(pt.size() + 16);
		unsigned long long outLen = 0;
		BOOST_REQUIRE(crypto_aead_xchacha20poly1305_ietf_encrypt(
			out.data(), &outLen, pt.data(), pt.size(),
			aad.data(), aad.size(), nullptr, nonce,
			encDec.GetCachedMasterKey().data()) == 0);
		out.resize(outLen);
		return out;
	};

	std::vector<uint8_t> pt(16, 0), ct16, tag;
	bool foundSingle = false, foundDouble = false;
	std::vector<uint8_t> ptSingle, ctSingle, tagSingle;
	std::vector<uint8_t> ptDouble, ctDouble, tagDouble;
	for (unsigned a = 0; a < 256 && (!foundSingle || !foundDouble); ++a)
	{
		pt[0] = (uint8_t)a;
		for (unsigned b = 0; b < 256; ++b)
		{
			pt[1] = (uint8_t)b;
			std::vector<uint8_t> out = encryptPt(pt);
			std::vector<uint8_t> c(out.begin(), out.begin() + 16);
			std::vector<uint8_t> t(out.begin() + 16, out.end());

			// The yEnc wire form of ct[0] must be a bare '.' (0x2E): ct[0] == 0x04
			// with ct[1] != 0x13 (= escape) so no '=' precedes it. For the double
			// case ct[0] == ct[1] == 0x04 with ct[2] != 0x13.
			bool bareSingle = c[0] == 0x04 && c[1] != 0x13;
			bool bareDouble = c[0] == 0x04 && c[1] == 0x04 && c[2] != 0x13;
			if (!foundSingle && bareSingle)
			{
				ptSingle = pt; ctSingle = c; tagSingle = t; foundSingle = true;
			}
			if (!foundDouble && bareDouble)
			{
				ptDouble = pt; ctDouble = c; tagDouble = t; foundDouble = true;
				break;
			}
		}
	}
	BOOST_REQUIRE(foundSingle);
	BOOST_REQUIRE(foundDouble);

	auto buildWire = [&](const std::vector<uint8_t>& ct, const std::vector<uint8_t>& tag,
		const std::vector<uint8_t>& expectedPt,
		std::vector<uint8_t>& expectedPlaintext) -> std::string
	{
		YEncDecryptor encDec(password);
		std::string line1Plain = "=ybegin line=128 size=16 name=dot.dat";
		std::vector<uint8_t> wireLine1;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
			segmentIndex, 1, true, salt.data(), wireLine1
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		char saltHex[33], tagHex[33];
		for (int i = 0; i < 16; ++i)
		{
			snprintf(saltHex + 2 * i, 3, "%02x", salt[i]);
			snprintf(tagHex + 2 * i, 3, "%02x", tag[i]);
		}
		std::string line2Plain = std::string(
			"=yencryption cipher=XChaCha20-Poly1305 salt=") + saltHex +
			" index=00000001 tag=" + tagHex;
		std::vector<uint8_t> wireLine2;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
			segmentIndex, 2, false, salt.data(), wireLine2
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		// yEnc-encode the ciphertext with full escaping (the simple yEncEncode
		// helper above does not escape '='), then apply RFC 3977 §3.1.1
		// dot-stuffing: a data line beginning with '.' gets one escape dot
		// prepended; the consumer's single unstuffing pass must remove it.
		std::string encLine;
		for (uint8_t c : ct)
		{
			uint8_t e = (uint8_t)((c + 42) % 256);
			if (e == 0x00 || e == 0x0A || e == 0x0D || e == 0x3D)
			{
				encLine += '=';
				e = (uint8_t)((e + 64) % 256);
			}
			encLine += (char)e;
		}
		BOOST_REQUIRE_EQUAL((int)(uint8_t)encLine[0], 0x2E); // '.'-leading data line
		std::string stuffedLine = "." + encLine;

		Crc32 wireCrc; wireCrc.Append(const_cast<uint8_t*>(ct.data()), ct.size());
	std::ostringstream crcFooter;
	crcFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << wireCrc.Finish();
	std::string line4Plain = crcFooter.str();
		std::vector<uint8_t> wireLine4;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
			segmentIndex, 4, false, salt.data(), wireLine4
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string fullWire;
		fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		fullWire.append("\r\n");
		fullWire.append(stuffedLine);
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
		fullWire.append("\r\n.\r\n");

		expectedPlaintext = expectedPt;
		return fullWire;
	};

	YEncDecryptor decryptor(password);

	// 1. Restored data line begins with '.'
	{
		std::vector<uint8_t> expectedPlaintext;
		std::string wire = buildWire(ctSingle, tagSingle, ptSingle, expectedPlaintext);
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL(decoder.GetSegmentIndex(), segmentIndex);
		BOOST_CHECK_EQUAL(decoder.GetSize(), 16);
		BOOST_CHECK_EQUAL(decoder.GetEndPos(), 16);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}

	// 2. Restored data line begins with '..' — a second unstuffing pass would
	//    collapse it to '.'.
	{
		std::vector<uint8_t> expectedPlaintext;
		std::string wire = buildWire(ctDouble, tagDouble, ptDouble, expectedPlaintext);
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL_COLLECTIONS(
			decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}
}

// A truncated encrypted article whose =yencryption metadata already
// arrived must report dsAuthFailed (provider failover tier),
// not dsArticleIncomplete.
BOOST_AUTO_TEST_CASE(TruncatedEncryptedArticleReportsAuthFailedTest)
{
	const std::string password = "conformance-fixture-password";
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const uint32_t segmentIndex = 1;
	YEncDecryptor encDec(password);

	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e";
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	// 1. Truncated before =yend: wire stream terminates abruptly
	{
		std::string wire;
		wire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		wire.append("\r\n");
		wire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		wire.append("\r\n");
		wire.append("someciphertextthatisneverfollowedbyyend\r\n");

		Decoder decoder;
		decoder.SetDecryptor(&encDec);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}

	// 2. Truncated ciphertext with valid =yend framing: size mismatch in CheckYenc
	// (m_endSize != m_outSize) must fail closed as dsAuthFailed, not dsInvalidSize.
	{
		std::vector<uint8_t> shortCt = YEncTest::HexToBin("6a0d1eb2"); // 4 bytes instead of 16
		std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(shortCt.data()), shortCt.size()));

		Crc32 wireCrc;
		wireCrc.Append(const_cast<uint8_t*>(shortCt.data()), shortCt.size());
		std::ostringstream crcFooter;
		crcFooter << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << wireCrc.Finish();
		std::string line4Plain = crcFooter.str();
		std::vector<uint8_t> wireLine4;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
			segmentIndex, 4, false, salt.data(), wireLine4
		)), static_cast<int>(YEncDecryptor::Status::Ok));

		std::string fullWire;
		fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
		fullWire.append("\r\n");
		fullWire.append(wireLine3);
		fullWire.append("\r\n");
		fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
		fullWire.append("\r\n.\r\n");

		Decoder decoder;
		decoder.SetDecryptor(&encDec);
		decoder.DecodeBuffer(fullWire.data(), static_cast<int>(fullWire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

BOOST_AUTO_TEST_CASE(OrdinaryYencLeadingDotTest)
{
	const std::string plainData = "\x04HelloUsenet";
	std::string enc = yEncEncode(plainData);
	BOOST_REQUIRE_EQUAL(static_cast<int>(static_cast<unsigned char>(enc[0])), 0x2E); // '.'

	Crc32 crc;
	crc.Append(reinterpret_cast<uchar*>(const_cast<char*>(plainData.data())), static_cast<uint32>(plainData.size()));
	uint32 expectedCrc = crc.Finish();

	std::stringstream endOss;
	endOss << "=yend size=" << plainData.size() << " crc32=" << std::hex << std::setw(8) << std::setfill('0') << expectedCrc;

	std::string wire;
	wire += "=ybegin line=128 size=" + std::to_string(plainData.size()) + " name=dot.dat\r\n";
	// Dot-stuffed wire data line (leading '.' escaped as '..'):
	wire += "." + enc + "\r\n";
	wire += endOss.str() + "\r\n";
	wire += ".\r\n";

	// 1. Ordinary unencrypted download without password configured
	{
		Decoder decoder;
		decoder.SetCrcCheck(true);
		std::string buf = wire;
		int len = decoder.DecodeBuffer(buf.data(), static_cast<int>(buf.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL(decoder.GetSize(), static_cast<int64>(plainData.size()));
		BOOST_CHECK_EQUAL(decoder.GetCalculatedCrc(), expectedCrc);
		BOOST_CHECK_EQUAL(len, static_cast<int>(plainData.size()));
		BOOST_CHECK_EQUAL(buf.substr(0, len), plainData);
	}

	// 2. Archive password present on an UNENCRYPTED job (decryptor not armed):
	// ArticleDownloader does not arm the decryptor for ordinary jobs (Body Std v1.2 §8).
	{
		Decoder decoder;
		decoder.SetDecryptor(nullptr);
		decoder.SetCrcCheck(true);
		std::string buf = wire;
		int len = decoder.DecodeBuffer(buf.data(), static_cast<int>(buf.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK_EQUAL(decoder.GetSize(), static_cast<int64>(plainData.size()));
		BOOST_CHECK_EQUAL(decoder.GetCalculatedCrc(), expectedCrc);
		BOOST_CHECK_EQUAL(len, static_cast<int>(plainData.size()));
		BOOST_CHECK_EQUAL(buf.substr(0, len), plainData);
	}

	// 3. Encrypted release armed (decryptor active): an unencrypted wire article
	// arriving with plaintext =ybegin is an unexpected fetched mode (provider corruption)
	// under combined-only reception . It must fail closed (dsAuthFailed).
	{
		Decoder decoder;
		YEncDecryptor decryptor("synthetic-wrong-fixture");
		decoder.SetDecryptor(&decryptor);
		std::string buf = wire;
		decoder.DecodeBuffer(buf.data(), static_cast<int>(buf.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
	}
}

BOOST_AUTO_TEST_CASE(EmbeddedTerminatorNotPrematurelyCompletedTest)
{
	const std::string password = "conformance-fixture-password";
	const uint32_t segmentIndex = 1;
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	YEncDecryptor encDec(password);

	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	// Build chunk 1 with valid encrypted line 1, followed by an embedded terminator and trailing data:
	std::string chunk1;
	chunk1.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	chunk1.append("\r\n\r\n.\r\nintermediate_data_after_terminator\r\n");

	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.DecodeBuffer(chunk1.data(), static_cast<int>(chunk1.size()));

	// An embedded terminator mid-stream must NOT complete the article prematurely:
	BOOST_CHECK_EQUAL(decoder.GetEof(), false);
}

BOOST_AUTO_TEST_CASE(EncryptedWireBadCrcFailClosedTest)
{
	// Valid tag, valid ciphertext, but corrupted wire CRC in =yend.
	// Even with CrcCheck=false (the user configured no CRC check), encrypted
	// wire mode MUST force CRC verification before AEAD, fail closed with
	// dsAuthFailed (provider corruption tier), and yield zero output.
	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::vector<uint8_t> ct = YEncTest::HexToBin("1136931a760f55f58a07bc9ce8329ede");
	std::vector<uint8_t> tag = YEncTest::HexToBin("da7a9fa260fa4d336ae8d139864d320e");
	uint32_t segmentIndex = 1;

	YEncDecryptor encDec(password);
	std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=da7a9fa260fa4d336ae8d139864d320e";
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wireLine3 = yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()));

	// Bad expected wire CRC in =yend (corrupted by provider)
	std::string line4Plain = "=yend size=16 crc32=deadbeef";
	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4Plain.data()), line4Plain.size(),
		segmentIndex, 4, false, salt.data(), wireLine4
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string fullWire;
	fullWire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size());
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size());
	fullWire.append("\r\n");
	fullWire.append(wireLine3);
	fullWire.append("\r\n");
	fullWire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size());
	fullWire.append("\r\n.\r\n");

	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.SetCrcCheck(false); // Explicitly disable ordinary CRC check!

	decoder.DecodeBuffer(fullWire.data(), static_cast<int>(fullWire.size()));

	// Must fail closed with dsAuthFailed and produce zero plaintext output
	auto status = decoder.Check();
	BOOST_CHECK_EQUAL(status, Decoder::dsAuthFailed);
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

namespace
{

// Body-vec-01 key/nonce (password conformance-fixture-password, salt 1a2b...7890, segment index 1).
const char* const BODY_VEC01_KEY = "7da24b7185cdf96dd32078b0fabfa34d141b282791cc641aa2d7afca3651d05f";
const char* const BODY_VEC01_NONCE = "e8198402caeb33fc4bae7eb8772a756d334c0176c02b9065";
const char* const SINGLE_PART_SALT = "1a2b3c4d5e6f7890abcdef1234567890";

// Body Std v1.2 section 2.1 AAD for a single-part article (part 1, begin 1, 16 segment bytes).
std::vector<uint8_t> SinglePartAad(const std::string& name)
{
	std::vector<uint8_t> aad = HexToBin("79656e632d626f647920616164" "00000001" "00000001" "0000000000000010");
	const uint32_t nameLen = static_cast<uint32_t>(name.size());
	aad.push_back(static_cast<uint8_t>(nameLen >> 24));
	aad.push_back(static_cast<uint8_t>(nameLen >> 16));
	aad.push_back(static_cast<uint8_t>(nameLen >> 8));
	aad.push_back(static_cast<uint8_t>(nameLen));
	aad.insert(aad.end(), name.begin(), name.end());
	return aad;
}

// Builds a complete encrypted single-part article (16-byte plaintext) with the given =ybegin name.
std::string BuildEncryptedSinglePartWire(const std::string& name, const std::vector<uint8_t>& plaintext, bool missingName = false, bool missingCrc = false)
{
	const std::vector<uint8_t> salt = HexToBin(SINGLE_PART_SALT);
	const std::vector<uint8_t> key = HexToBin(BODY_VEC01_KEY);
	const std::vector<uint8_t> nonce = HexToBin(BODY_VEC01_NONCE);
	const std::vector<uint8_t> aad = SinglePartAad(name);

	std::vector<uint8_t> ct(plaintext.size());
	std::vector<uint8_t> tag(crypto_aead_xchacha20poly1305_ietf_ABYTES);
	crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ct.data(), tag.data(), nullptr,
		plaintext.data(), plaintext.size(), aad.data(), aad.size(), nullptr, nonce.data(), key.data());

	YEncDecryptor encDec("conformance-fixture-password");
	std::string line1 = "=ybegin line=128 size=16" + (missingName ? std::string() : " name=" + name);
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1.data()), line1.size(), 1, 1, true, salt.data(), wireLine1)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2 = std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") + SINGLE_PART_SALT +
		" index=00000001 tag=" + BinToHex(tag);
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2.data()), line2.size(), 1, 2, false, salt.data(), wireLine2)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	Crc32 crc;
	crc.Append(ct.data(), ct.size());
	const uint32_t ctCrc = crc.Finish();
	std::ostringstream endOss;
	endOss << "=yend size=16 crc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc;
	std::string line4 = missingCrc ? "=yend size=16" : endOss.str();
	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4.data()), line4.size(), 1, 4, false, salt.data(), wireLine4)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wire;
	wire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size()).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size()).append("\r\n");
	wire.append(yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()))).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size()).append("\r\n.\r\n");
	return wire;
}

// Body Std v1.2 section 5 (CRC token grammar): builds the same valid encrypted
// single-part article as BuildEncryptedSinglePartWire, but the restored =yend
// crc32 token is supplied verbatim so tests can exercise the token grammar
// while ciphertext, tag and AAD stay valid.
std::string BuildEncryptedSinglePartWireWithCrcToken(const std::string& crcToken, const std::vector<uint8_t>& plaintext)
{
	const std::vector<uint8_t> salt = HexToBin(SINGLE_PART_SALT);
	const std::vector<uint8_t> key = HexToBin(BODY_VEC01_KEY);
	const std::vector<uint8_t> nonce = HexToBin(BODY_VEC01_NONCE);
	const std::vector<uint8_t> aad = SinglePartAad("test.dat");

	std::vector<uint8_t> ct(plaintext.size());
	std::vector<uint8_t> tag(crypto_aead_xchacha20poly1305_ietf_ABYTES);
	crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ct.data(), tag.data(), nullptr,
		plaintext.data(), plaintext.size(), aad.data(), aad.size(), nullptr, nonce.data(), key.data());

	YEncDecryptor encDec("conformance-fixture-password");
	std::string line1 = "=ybegin line=128 size=16 name=test.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1.data()), line1.size(), 1, 1, true, salt.data(), wireLine1)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2 = std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") + SINGLE_PART_SALT +
		" index=00000001 tag=" + BinToHex(tag);
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2.data()), line2.size(), 1, 2, false, salt.data(), wireLine2)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line4 = "=yend size=16 crc32=" + crcToken;
	std::vector<uint8_t> wireLine4;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line4.data()), line4.size(), 1, 4, false, salt.data(), wireLine4)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wire;
	wire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size()).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size()).append("\r\n");
	wire.append(yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()))).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine4.data()), wireLine4.size()).append("\r\n.\r\n");
	return wire;
}

// Body Std v1.2 section 5 (CRC token grammar): builds a valid encrypted
// multipart part article (part 1 of 2, name multi.dat) whose restored =yend
// pcrc32 token is supplied verbatim; wholeFileCrcToken, when non-empty,
// appends a whole-file " crc32=<token>" which receivers must ignore.
// plaintext defaults to the base fixture payload (ciphertext CRC 42ad0236);
// the short-token case passes a payload whose ciphertext CRC is 00f19c10ad.
std::string BuildEncryptedMultipartWireWithCrcToken(const std::string& pcrcToken,
	const std::string& wholeFileCrcToken = "",
	const std::vector<uint8_t>& pt = HexToBin("48656c6c6f20576f726c642e747874ff"),
	const std::string& ybeginLine = "=ybegin part=1 total=2 line=128 size=32 name=multi.dat",
	const std::string& ypartLine = "=ypart begin=1 end=16")
{
	const std::string name = "multi.dat";
	const std::vector<uint8_t> salt = HexToBin(SINGLE_PART_SALT);
	const std::vector<uint8_t> key = HexToBin(BODY_VEC01_KEY);
	const std::vector<uint8_t> nonce = HexToBin(BODY_VEC01_NONCE);
	// Part 1, begin 1, 16 segment bytes: the same AAD shape as the single-part builder.
	const std::vector<uint8_t> aad = SinglePartAad(name);

	std::vector<uint8_t> ct(pt.size());
	std::vector<uint8_t> tag(crypto_aead_xchacha20poly1305_ietf_ABYTES);
	crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ct.data(), tag.data(), nullptr,
		pt.data(), pt.size(), aad.data(), aad.size(), nullptr, nonce.data(), key.data());

	YEncDecryptor encDec("conformance-fixture-password");
	std::string line1 = ybeginLine;
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1.data()), line1.size(), 1, 1, true, salt.data(), wireLine1)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2 = ypartLine;
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2.data()), line2.size(), 1, 2, false, salt.data(), wireLine2)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line3 = std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") + SINGLE_PART_SALT +
		" index=00000001 tag=" + BinToHex(tag);
	std::vector<uint8_t> wireLine3;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line3.data()), line3.size(), 1, 3, false, salt.data(), wireLine3)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line5 = "=yend size=16 part=1 pcrc32=" + pcrcToken;
	if (!wholeFileCrcToken.empty())
	{
		line5 += " crc32=" + wholeFileCrcToken;
	}
	std::vector<uint8_t> wireLine5;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line5.data()), line5.size(), 1, 5, false, salt.data(), wireLine5)),
		static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wire;
	wire.append(reinterpret_cast<const char*>(wireLine1.data()), wireLine1.size()).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine2.data()), wireLine2.size()).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine3.data()), wireLine3.size()).append("\r\n");
	wire.append(yEncEncode(std::string(reinterpret_cast<const char*>(ct.data()), ct.size()))).append("\r\n");
	wire.append(reinterpret_cast<const char*>(wireLine5.data()), wireLine5.size()).append("\r\n.\r\n");
	return wire;
}

} // namespace

// Body Std v1.2 section 2.1 / Control Std v1.2 section 5: an encrypted single-part article whose
// =ybegin name contains " part=" is single-part. The AAD must use part 1 and begin 1.
BOOST_AUTO_TEST_CASE(EncryptedSinglePartNameContainsPartTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string wire = BuildEncryptedSinglePartWire("my part=2.bin", expectedPlaintext);

	YEncDecryptor encDec("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.SetCrcCheck(true);
	std::string msg = wire;
	decoder.DecodeBuffer(msg.data(), static_cast<int>(msg.size()));

	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFinished));
	BOOST_CHECK_EQUAL(std::string(decoder.GetArticleFilename()), "my part=2.bin");
	BOOST_CHECK(decoder.IsEncrypted());
	BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
		expectedPlaintext.begin(), expectedPlaintext.end());
}

// Control Std v1.2 section 3/5: name= runs to end of line, so "=ypart " and " begin=" inside an encrypted
// name are file name text. The AAD must keep part 1 and begin 1 (not begin=5 from the name).
BOOST_AUTO_TEST_CASE(EncryptedSinglePartNameContainsYpartBeginTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string wire = BuildEncryptedSinglePartWire("a =ypart x begin=5.bin", expectedPlaintext);

	YEncDecryptor encDec("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.SetCrcCheck(true);
	std::string msg = wire;
	decoder.DecodeBuffer(msg.data(), static_cast<int>(msg.size()));

	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFinished));
	BOOST_CHECK_EQUAL(std::string(decoder.GetArticleFilename()), "a =ypart x begin=5.bin");
	BOOST_CHECK(decoder.IsEncrypted());
	BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
		expectedPlaintext.begin(), expectedPlaintext.end());
}

// An unencrypted article with the same name keeps the upstream strstr(" part=") decoding.
BOOST_AUTO_TEST_CASE(PlainSinglePartNameContainsPartUnchangedTest)
{
	const std::string payload = "Hello";
	const std::string encoded = yEncEncode(payload);
	std::vector<uint8_t> payloadBytes(payload.begin(), payload.end());
	Crc32 crc;
	crc.Append(payloadBytes.data(), payloadBytes.size());
	std::ostringstream endOss;
	endOss << "=yend size=5 crc32=" << std::hex << std::setw(8) << std::setfill('0') << crc.Finish();
	const std::string wire = "=ybegin line=128 size=5 name=my part=2.bin\r\n" + encoded + "\r\n" + endOss.str() +
		"\r\n.\r\n";

	Decoder decoder;
	std::string msg = wire;
	const int len = decoder.DecodeBuffer(msg.data(), static_cast<int>(msg.size()));
	// Upstream plain yEnc: strstr(" part=") also matches inside name=, so the article is treated as a
	// multipart part (part 2) without =ypart and rejected as invalid size. Output stays empty.
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsInvalidSize));
	BOOST_CHECK_EQUAL(decoder.GetSize(), 5);
	BOOST_CHECK_EQUAL(len, 0);
	BOOST_CHECK(!decoder.IsEncrypted());
}

// Control Std v1.2 section 5: physical line 2 decides placement even when name= is missing.
// Line 1 is encrypted with segment index 1; the body line is passed through verbatim.
BOOST_AUTO_TEST_CASE(RestoreControlLinesNameAbsentPartDecidesLayoutTest)
{
	YEncDecryptor encDec("conformance-fixture-password");
	const std::vector<uint8_t> salt = HexToBin(SINGLE_PART_SALT);
	auto encrypt = [&](const std::string& text, uint32_t lineIndex, bool line1)
	{
		std::vector<uint8_t> out;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(text.data()), text.size(), 1, lineIndex, line1, salt.data(), out)),
			static_cast<int>(YEncDecryptor::Status::Ok));
		return std::string(out.begin(), out.end());
	};
	const std::string header = std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") + SINGLE_PART_SALT +
		" index=00000001 tag=00000000000000000000000000000000";

	// Single-part, no name= and no part=: four lines, no =ypart.
	{
		const std::string wire = encrypt("=ybegin line=128 size=16", 1, true) + "\r\n" +
			encrypt(header, 2, false) + "\r\n" + "F\r\n" + encrypt("=yend size=16 crc32=00000000", 4, false) + "\r\n";
		YEncDecryptor::YEncryptionHeader parsed;
		std::string clean;
		std::vector<uint8_t> outSalt;
		BOOST_REQUIRE(encDec.RestoreControlLines(wire.data(), wire.size(), clean, outSalt, &parsed) == YEncDecryptor::Status::Ok);
		BOOST_CHECK_EQUAL(clean, "=ybegin line=128 size=16\r\nF\r\n=yend size=16 crc32=00000000\r\n");
		BOOST_CHECK_EQUAL(parsed.segmentIndex, 1u);
	}

	// Multipart by part= alone (no name=): five lines, =ypart at line 2.
	{
		const std::string wire = encrypt("=ybegin part=1 total=1 line=128 size=16", 1, true) + "\r\n" +
			encrypt("=ypart begin=1 end=16", 2, false) + "\r\n" + encrypt(header, 3, false) + "\r\n" + "F\r\n" +
			encrypt("=yend size=16 crc32=00000000", 5, false) + "\r\n";
		std::string clean;
		std::vector<uint8_t> outSalt;
		BOOST_REQUIRE(encDec.RestoreControlLines(wire.data(), wire.size(), clean, outSalt) == YEncDecryptor::Status::Ok);
		BOOST_CHECK(clean.find("=ypart begin=1 end=16\r\n") != std::string::npos);
		BOOST_CHECK(clean.find("=yencryption") == std::string::npos);
	}
}

// Body Std v1.2 §2.1 (GAP13-05): on the encrypted path, duplicate
// part=/size= (=ybegin) or begin=/end= (=ypart) keywords must all be
// well-formed and equal; same-value duplicates decode, conflicting or
// malformed duplicates fail closed with no plaintext. AAD stays valid
// (part 1, begin 1, 16 segment bytes) in every case below.
BOOST_AUTO_TEST_CASE(EncryptedDuplicateGeometryKeywordsTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	const std::string goodBegin = "=ybegin part=1 total=2 line=128 size=32 name=multi.dat";
	const std::string goodPart = "=ypart begin=1 end=16";

	auto runCase = [&](const std::string& ybeginLine, const std::string& ypartLine,
		Decoder::EStatus expectedStatus)
	{
		std::string wire = BuildEncryptedMultipartWireWithCrcToken("42ad0236", "", expectedPlaintext,
			ybeginLine, ypartLine);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(expectedStatus));
		if (expectedStatus == Decoder::dsFinished)
		{
			BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
				expectedPlaintext.begin(), expectedPlaintext.end());
		}
		else
		{
			BOOST_CHECK(decoder.GetDecryptedData().empty());
		}
	};

	// Controls: canonical lines and same-value duplicates of all four
	// geometry keywords (both orders where the value set allows it).
	runCase(goodBegin, goodPart, Decoder::dsFinished);
	runCase("=ybegin part=1 part=1 total=2 line=128 size=32 size=32 name=multi.dat", goodPart, Decoder::dsFinished);
	runCase("=ybegin part=1 total=2 line=128 size=32 name=multi.dat", "=ypart begin=1 begin=1 end=16 end=16", Decoder::dsFinished);

	// Conflicting duplicates in both orders; each must fail closed.
	runCase("=ybegin part=1 part=2 total=2 line=128 size=32 name=multi.dat", goodPart, Decoder::dsAuthFailed);
	runCase("=ybegin part=2 part=1 total=2 line=128 size=32 name=multi.dat", goodPart, Decoder::dsAuthFailed);
	runCase("=ybegin part=1 total=2 line=128 size=32 size=33 name=multi.dat", goodPart, Decoder::dsAuthFailed);
	runCase("=ybegin part=1 total=2 line=128 size=33 size=32 name=multi.dat", goodPart, Decoder::dsAuthFailed);
	runCase(goodBegin, "=ypart begin=1 begin=2 end=16", Decoder::dsAuthFailed);
	runCase(goodBegin, "=ypart begin=2 begin=1 end=16", Decoder::dsAuthFailed);
	runCase(goodBegin, "=ypart begin=1 end=16 end=17", Decoder::dsAuthFailed);
	runCase(goodBegin, "=ypart begin=1 end=17 end=16", Decoder::dsAuthFailed);

	// Malformed or overflowing occurrence beside a valid duplicate.
	runCase("=ybegin part=1 part=abc total=2 line=128 size=32 name=multi.dat", goodPart, Decoder::dsAuthFailed);
	runCase(goodBegin, "=ypart begin=1 begin=4294967296 end=16", Decoder::dsAuthFailed);
	runCase("=ybegin part=1 total=2 line=128 size=32 size=4294967296 name=multi.dat", goodPart, Decoder::dsAuthFailed);
}

// Body Std v1.2 §5 (GAP13-01): a duplicate =yencryption line in the data
// region (after the first data line) fails closed; the implementation is
// YEncDecryptor::RestoreControlLines' body scan. The duplicate here is a
// plaintext line in the restored block (a wire-encrypted duplicate would
// simply look like ordinary data after the first data line started).
BOOST_AUTO_TEST_CASE(EncryptedDuplicateYencryptionLineFailsClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	std::string wire = BuildEncryptedSinglePartWire("file.bin", expectedPlaintext);
	// Insert a plaintext duplicate =yencryption line after the first data
	// line. The restored block then contains it verbatim in the body region;
	// split the single encoded data line so the duplicate really sits after
	// a data line (splitting yEnc data at a line boundary is inert).
	size_t line1End = wire.find("\r\n");
	BOOST_REQUIRE(line1End != std::string::npos);
	size_t line2End = wire.find("\r\n", line1End + 2);
	BOOST_REQUIRE(line2End != std::string::npos);
	size_t dataStart = line2End + 2;
	size_t dataEnd = wire.find("\r\n", dataStart);
	BOOST_REQUIRE(dataEnd != std::string::npos && dataEnd > dataStart);
	std::string data = wire.substr(dataStart, dataEnd - dataStart);
	size_t split = data.size() / 2;
	BOOST_REQUIRE(split > 0);
	std::string mutated = wire.substr(0, dataStart) + data.substr(0, split) + "\r\n" +
		"=yencryption cipher=XChaCha20-Poly1305\r\n" + data.substr(split) + wire.substr(dataEnd);
	YEncDecryptor decryptor("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);
	decoder.DecodeBuffer(mutated.data(), static_cast<int>(mutated.size()));
	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
	BOOST_CHECK(decoder.GetDecryptedData().empty());
}

// Body Std v1.2 §2.1/§5 (GAP13-02): enumerated fields at exactly 2^32
// (begin=, end=, =ybegin size=) fail closed. begin=4294967296 must not wrap
// to 0 into the AAD geometry check (the raw value never reaches CheckYenc
// masked: the encrypted duplicate/decimal grammar rejects it first; for
// end= the CheckYenc >= 2^32 guard fires). AAD stays valid in all cases.
BOOST_AUTO_TEST_CASE(EncryptedEnumeratedFieldOverflowFailsClosedTest)
{
	const std::string goodBegin = "=ybegin part=1 total=2 line=128 size=32 name=multi.dat";
	const std::string goodPart = "=ypart begin=1 end=16";
	const char* overflowYbegins[] = {
		"=ybegin part=1 total=2 line=128 size=4294967296 name=multi.dat",
	};
	const char* overflowYparts[] = {
		"=ypart begin=4294967296 end=16",
		"=ypart begin=1 end=4294967296",
	};
	for (const char* ybeginLine : overflowYbegins)
	{
		std::string wire = BuildEncryptedMultipartWireWithCrcToken("42ad0236", "",
			HexToBin("48656c6c6f20576f726c642e747874ff"), std::string(ybeginLine), goodPart);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
	for (const char* ypartLine : overflowYparts)
	{
		std::string wire = BuildEncryptedMultipartWireWithCrcToken("42ad0236", "",
			HexToBin("48656c6c6f20576f726c642e747874ff"), goodBegin, std::string(ypartLine));
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// NG12-1/NG12-2: trailing separators after the footer are outside the
// block (STD12-2) and must not shift the footer's lineIndex; an oversized
// non-Line-1 control line is a local (fatal) failure, not provider
// corruption, so it surfaces as dsFatalError instead of dsAuthFailed.
BOOST_AUTO_TEST_CASE(EncryptedTrailingBlankLinesAfterFooterTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	std::string wire = BuildEncryptedSinglePartWire("file.bin", expectedPlaintext);
	BOOST_REQUIRE(wire.ends_with("\r\n.\r\n"));
	wire.insert(wire.size() - 3, "\r\n\r\n");
	YEncDecryptor decryptor("conformance-fixture-password");
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);
	decoder.SetCrcCheck(true);
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
	BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
		expectedPlaintext.begin(), expectedPlaintext.end());
}

// NG12-2: AuthenticateAndDecrypt's local failures (here: empty password
// makes EnsureMasterKey fail) are fatal, while a bad tag stays AuthFailed.
BOOST_AUTO_TEST_CASE(EncryptedLocalCryptoFailureIsFatalTest)
{
	YEncDecryptor decryptor("");
	const std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	const std::vector<uint8_t> tag = HexToBin("0cd77ce245a654463f90b945b1d22d5b");
	const std::vector<uint8_t> aad = YEncDecryptor::BuildAad("file.bin", 1, 1, 1);
	const uint8_t ct[1] = {0};
	std::vector<uint8_t> out;
	BOOST_CHECK(decryptor.AuthenticateAndDecrypt(ct, sizeof(ct), salt.data(), tag.data(), 1,
		aad.data(), aad.size(), out) == YEncDecryptor::Status::Error);
	BOOST_CHECK(decryptor.WasLastErrorFatal());
	BOOST_CHECK(out.empty());
}

// NG12-3: an encrypted multipart part= at or above 2^32 fails closed as
// provider corruption; the parsed value never wraps into the AAD. The
// ordinary path keeps its upstream atoi behaviour.
BOOST_AUTO_TEST_CASE(EncryptedPartOverflowFailsClosedTest)
{
	// Build a full encrypted multipart article whose restored =ybegin
	// carries part=4294967296 (>= 2^32).
	const std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	YEncDecryptor encDec("conformance-fixture-password");
	auto encrypt = [&](const std::string& text, uint32_t lineIndex, bool line1)
	{
		std::vector<uint8_t> out;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(text.data()), text.size(), 1, lineIndex, line1, salt.data(), out)),
			static_cast<int>(YEncDecryptor::Status::Ok));
		return std::string(out.begin(), out.end());
	};
	const std::string header = std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") +
		"1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=00000000000000000000000000000000";
	const std::string wire =
		encrypt("=ybegin part=4294967296 total=2 line=128 size=32 name=multi.dat", 1, true) + "\r\n" +
		encrypt("=ypart begin=1 end=16", 2, false) + "\r\n" +
		encrypt(header, 3, false) + "\r\n" + "F\r\n" +
		encrypt("=yend size=16 part=4294967296 pcrc32=00000000", 5, false) + "\r\n.\r\n";
	Decoder decoder;
	decoder.SetDecryptor(&encDec);
	decoder.DecodeBuffer(const_cast<char*>(wire.data()), static_cast<int>(wire.size()));
	BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
	BOOST_CHECK(decoder.GetDecryptedData().empty());

	// Ordinary path is untouched: it still uses atoi() exactly as upstream.
	Decoder plain;
	const std::string plainLine = "=ybegin part=4294967296 total=2 line=128 size=32 name=multi.dat\r\n";
	plain.DecodeBuffer(const_cast<char*>(plainLine.data()), static_cast<int>(plainLine.size()));
	BOOST_CHECK_EQUAL(plain.GetPartNumber(), atoi("4294967296"));
}

BOOST_AUTO_TEST_CASE(EncryptedMissingNameAndCrcFailClosedTest)
{
	const std::vector<uint8_t> payload = HexToBin("48656c6c6f20576f726c642e747874ff");
	for (int defect = 0; defect < 3; ++defect)
	{
		std::string wire = BuildEncryptedSinglePartWire(defect == 1 ? "" : "file.bin", payload, defect == 0, defect == 2);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// Body Std v1.2 section 5: the applicable =yend CRC token must be a hex
// string that parses entirely as a CRC-32 value. A correct hex prefix plus
// non-hex garbage ("<correct>z") is invalid and must fail under
// PROVIDER_FAILOVER before AEAD authentication, for both the single-part
// crc32 token and the multipart pcrc32 token.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenTrailingGarbageFailClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	// Correct CRC prefix of the fixture ciphertext is 42ad0236.
	for (bool multipart : {false, true})
	{
		std::string wire = multipart
			? BuildEncryptedMultipartWireWithCrcToken("42ad0236z")
			: BuildEncryptedSinglePartWireWithCrcToken("42ad0236z", expectedPlaintext);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// Body Std v1.2 section 5: a CRC token larger than 0xffffffff is invalid.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenRangeOverflowFailClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	for (bool multipart : {false, true})
	{
		// 0x100000000 > 0xffffffff (its low 32 bits would also mismatch).
		std::string wire = multipart
			? BuildEncryptedMultipartWireWithCrcToken("100000000")
			: BuildEncryptedSinglePartWireWithCrcToken("100000000", expectedPlaintext);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// Body Std v1.2 section 5: an empty applicable CRC token is invalid, even
// when the whole expected value would be zero.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenEmptyFailClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	for (bool multipart : {false, true})
	{
		std::string wire = multipart
			? BuildEncryptedMultipartWireWithCrcToken("")
			: BuildEncryptedSinglePartWireWithCrcToken("", expectedPlaintext);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// Body Std v1.2 section 5: a CRC token is hexadecimal only; '+', '-' and
// "0x" prefixes accepted by strtoul are invalid.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenPrefixesFailClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	for (const std::string& token : {"+42ad0236", "-42ad0236", "0x42ad0236"})
	{
		for (bool multipart : {false, true})
		{
			std::string wire = multipart
				? BuildEncryptedMultipartWireWithCrcToken(token)
				: BuildEncryptedSinglePartWireWithCrcToken(token, expectedPlaintext);
			YEncDecryptor decryptor("conformance-fixture-password");
			Decoder decoder;
			decoder.SetDecryptor(&decryptor);
			decoder.SetCrcCheck(true);
			decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
			BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
			BOOST_CHECK(decoder.GetDecryptedData().empty());
		}
	}
}

// Body Std v1.2 section 5: a CRC token whose hex prefix matches the correct
// value plus an overflow digit ("142ad0236" = 0x142ad0236) is invalid; its
// strtoul-uint32 truncation must not silently compare equal to the CRC.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenTruncatingPrefixFailClosedTest)
{
	const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
	for (bool multipart : {false, true})
	{
		std::string wire = multipart
			? BuildEncryptedMultipartWireWithCrcToken("142ad0236")
			: BuildEncryptedSinglePartWireWithCrcToken("142ad0236", expectedPlaintext);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsAuthFailed);
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}
}

// Body Std v1.2 section 5: any-width valid hex is accepted. Mixed case,
// a canonical short (non zero-padded) token, a zero-padded token and a wide
// leading-zero token must all still authenticate and decrypt.
BOOST_AUTO_TEST_CASE(EncryptedCrcTokenValidWidthsAndCaseTest)
{
	// Fixture plaintext: its ciphertext CRC32 is 42ad0236 (uppercase 42AD0236
	// valid, zero-padded 0000000042ad0236 valid).
	const std::vector<uint8_t> payload = HexToBin("48656c6c6f20576f726c642e747874ff");
	// Variant plaintext: its ciphertext CRC32 is 00f19c10ad, so the canonical
	// non zero-padded token is the 6-digit "f19c10ad".
	const std::vector<uint8_t> shortPayload = HexToBin("48656c6c6f20576f726c642e74780022");
	for (bool multipart : {false, true})
	{
		for (const auto& [token, plaintext] : std::vector<std::pair<std::string, const std::vector<uint8_t>*>>{
			{"42AD0236", &payload},
			{"42ad0236", &payload},
			{"0000000042ad0236", &payload},
			{"00f19c10ad", &shortPayload},
			{"f19c10ad", &shortPayload},
			{"000000000000f19c10ad", &shortPayload}})
		{
			std::string wire = multipart
				? BuildEncryptedMultipartWireWithCrcToken(token, "", *plaintext)
				: BuildEncryptedSinglePartWireWithCrcToken(token, *plaintext);
			YEncDecryptor decryptor("conformance-fixture-password");
			Decoder decoder;
			decoder.SetDecryptor(&decryptor);
			decoder.SetCrcCheck(true);
			decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
			BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
			BOOST_CHECK(decoder.IsEncrypted());
			BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
				plaintext->begin(), plaintext->end());
		}
	}
}

// Body Std v1.2 section 5: a multipart =yend carries only pcrc32; a whole-file
// crc32, even a malformed one, must stay ignored so the article still decodes.
BOOST_AUTO_TEST_CASE(EncryptedMultipartWholeFileCrcTokenIgnoredTest)
{
	for (const std::string& malformed : {"100000000", "deadbeefz", "+42ad0236", ""})
	{
		std::string wire = BuildEncryptedMultipartWireWithCrcToken("42ad0236", malformed);
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		decoder.SetCrcCheck(true);
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(decoder.Check(), Decoder::dsFinished);
		BOOST_CHECK(decoder.IsEncrypted());
		const std::vector<uint8_t> expectedPlaintext = HexToBin("48656c6c6f20576f726c642e747874ff");
		BOOST_CHECK_EQUAL_COLLECTIONS(decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
			expectedPlaintext.begin(), expectedPlaintext.end());
	}
}

// Body Std v1.2: an encrypted restored block whose
// yEnc body contains an interior data line consisting of a single '.' (sent
// dot-stuffed as '..' per RFC 3977 §3.1.1) followed by more data lines must
// decode in full. DecodeYenc's wire-mode scan for the transport terminator
// must not fire here, because ProcessRestoredBlock feeds an already-unstuffed
// block back through DecodeBuffer while m_encryptedWireMode is still set.
BOOST_AUTO_TEST_CASE(EncryptedWireRestoredBlockInteriorDotLineTest)
{
	const std::string password = "conformance-fixture-password";
	const uint32_t segmentIndex = 1;
	const std::vector<uint8_t> salt = YEncTest::HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	YEncDecryptor encDec(password);
	BOOST_REQUIRE(encDec.EnsureMasterKey(salt.data()));

	uint8_t nonce[24];
	BOOST_REQUIRE(encDec.DeriveBodyNonce(segmentIndex, nonce));

	// The full ciphertext: line1 = "A"*40 (so its yEnc wire form does not
	// begin with '.'), line2 = "." (wire '.', i.e. transported as '..'),
	// line3 = "B"*40, then CRLF.
	std::vector<uint8_t> pt;
	pt.insert(pt.end(), 40, 'A');
	pt.push_back('.');
	pt.insert(pt.end(), 40, 'B');
	pt.push_back('\r');
	pt.push_back('\n');

	const std::string name = "interiordot.dat";
	std::vector<uint8_t> aad = YEncDecryptor::BuildAad(name, 1, 1, pt.size());
	std::vector<uint8_t> ct(pt.size() + 16);
	std::vector<uint8_t> tag(16);
	unsigned long long outLen = 0;

	// Brute-force pt[40] so the ciphertext byte at index 40 is 0x04: its
	// yEnc wire form is then a bare '.' (0x2E), which we can carve out as an
	// interior data line consisting of exactly one dot. XChaCha20 is a stream
	// cipher, so pt[40] only affects ct[40].
	BOOST_REQUIRE_EQUAL((int)pt[40], (int)'.');
	for (unsigned a = 0; a < 256; ++a)
	{
		pt[40] = (uint8_t)a;
		BOOST_REQUIRE(crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
			ct.data(), tag.data(), nullptr, pt.data(), pt.size(),
			aad.data(), aad.size(), nullptr, nonce, encDec.GetCachedMasterKey().data()) == 0);
		if (ct[40] == 0x04)
		{
			break;
		}
	}
	BOOST_REQUIRE_EQUAL((int)ct[40], 0x04);
	BOOST_REQUIRE_NE((int)ct[41], 0x13); // ct[40]'s wire byte must not be '='-escaped
	ct.resize(pt.size());

	// Encrypt the control lines (Body Std v1.2 / Control Std v1.2 §5).
	auto encryptLine = [&](const std::string& text, uint32_t lineIndex, bool line1)
	{
		std::vector<uint8_t> out;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(text.data()), text.size(),
			segmentIndex, lineIndex, line1, salt.data(), out)),
			static_cast<int>(YEncDecryptor::Status::Ok));
		return std::string(out.begin(), out.end());
	};

	const std::string wireLine1 = encryptLine(
		"=ybegin line=128 size=" + std::to_string(pt.size()) + " name=" + name, 1, true);
	char saltHex[33], tagHex[33];
	for (int i = 0; i < 16; ++i)
	{
		snprintf(saltHex + 2 * i, 3, "%02x", salt[i]);
		snprintf(tagHex + 2 * i, 3, "%02x", tag[i]);
	}
	const std::string wireLine2 = encryptLine(
		std::string("=yencryption cipher=XChaCha20-Poly1305 salt=") + saltHex +
			" index=00000001 tag=" + tagHex,
		2, false);

	// yEnc-encode the ciphertext with full escaping, then split the encoded
	// body at ct[40]'s wire byte so the middle physical line is exactly ".".
	BOOST_REQUIRE_NE((int)(uint8_t)ct[0], 0x04); // first wire line must not begin with '.'
	std::string enc;
	size_t dotPos = std::string::npos;
	for (size_t i = 0; i < ct.size(); ++i)
	{
		uint8_t e = (uint8_t)((ct[i] + 42) % 256);
		if (e == 0x00 || e == 0x0A || e == 0x0D || e == 0x3D)
		{
			enc += '=';
			e = (uint8_t)((e + 64) % 256);
		}
		if (i == 40)
		{
			BOOST_REQUIRE_EQUAL((int)(uint8_t)e, 0x2E);
			dotPos = enc.size();
		}
		enc += (char)e;
	}
	BOOST_REQUIRE_NE(dotPos, std::string::npos);
	std::string body = enc.substr(0, dotPos) + "\r\n.\r\n" + enc.substr(dotPos + 1) + "\r\n";

	Crc32 ctCrc;
	ctCrc.Append(ct.data(), ct.size());
	std::ostringstream endOss;
	endOss << "=yend size=" << pt.size() << " crc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc.Finish();
	// RestoreControlLines decrypts the footer with lineIndex = lines.size():
	// 6 physical lines (ybegin, yencryption, bodyA, ".", bodyB, yend).
	const std::string wireLine4 = encryptLine(endOss.str(), 6, false);

	auto buildWire = [&]()
	{
		// Wire transport form: the interior "." data line is dot-stuffed to
		// ".." per RFC 3977 §3.1.1 (the consumer's single unstuffing pass
		// restores it to "." inside the accumulated raw block).
		std::string wire;
		wire.append(wireLine1).append("\r\n");
		wire.append(wireLine2).append("\r\n");
		wire.append(enc.substr(0, dotPos)).append("\r\n..\r\n").append(enc.substr(dotPos + 1)).append("\r\n");
		wire.append(wireLine4).append("\r\n.\r\n");
		return wire;
	};

	std::string wire = buildWire();

	// Control: the identical article without the interior dot line (whole
	// encoded body as one data line) must decode fine, proving the fixture
	// and CRC are sound and the failure is caused solely by the interior
	// dot line. Its footer carries lineIndex 4 (4 content lines).
	{
		const std::string wireLine4c = encryptLine(endOss.str(), 4, false);
		std::string wireControl;
		wireControl.append(wireLine1).append("\r\n");
		wireControl.append(wireLine2).append("\r\n");
		wireControl.append(enc).append("\r\n");
		wireControl.append(wireLine4c).append("\r\n.\r\n");
		YEncDecryptor decryptor2(password);
		Decoder decoder2;
		decoder2.SetDecryptor(&decryptor2);
		decoder2.SetCrcCheck(true);
		decoder2.DecodeBuffer(wireControl.data(), static_cast<int>(wireControl.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder2.Check()), static_cast<int>(Decoder::dsFinished));
		BOOST_CHECK_EQUAL_COLLECTIONS(
			decoder2.GetDecryptedData().begin(), decoder2.GetDecryptedData().end(),
			pt.begin(), pt.end());
	}

	// Chunked feeding (byte-at-a-time across the stuffed '..') must behave
	// identically to the single-shot feed.
	{
		YEncDecryptor decryptor3(password);
		Decoder decoder3;
		decoder3.SetDecryptor(&decryptor3);
		decoder3.SetCrcCheck(true);
		for (size_t i = 0; i < wire.size(); ++i)
		{
			decoder3.DecodeBuffer(wire.data() + i, 1);
		}
		BOOST_CHECK_EQUAL(static_cast<int>(decoder3.Check()), static_cast<int>(Decoder::dsFinished));
		BOOST_CHECK_EQUAL_COLLECTIONS(
			decoder3.GetDecryptedData().begin(), decoder3.GetDecryptedData().end(),
			pt.begin(), pt.end());
	}

	YEncDecryptor decryptor(password);
	Decoder decoder;
	decoder.SetDecryptor(&decryptor);
	decoder.SetCrcCheck(true);
	decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));

	BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsFinished));
	BOOST_CHECK(decoder.IsEncrypted());
	BOOST_CHECK_EQUAL(decoder.GetSegmentIndex(), segmentIndex);
	BOOST_CHECK_EQUAL_COLLECTIONS(
		decoder.GetDecryptedData().begin(), decoder.GetDecryptedData().end(),
		pt.begin(), pt.end());
}

// NZ-NEXT-1 / T05: a synthetic early EOF (encrypted-rejection path in
// DecodeBuffer) fires before the transport terminator arrives. The decoder
// must report that the multiline response was not fully consumed so the
// connection owner can disconnect instead of reusing a desynchronized socket.
// Ordinary articles always set m_wireTerminated once the terminator arrives;
// this guard only closes the undrained encrypted-rejection gap.
BOOST_AUTO_TEST_CASE(EncryptedEarlyRejectionReportsUnconsumedTerminatorTest)
{
	// 1. Plaintext =ybegin Line 1 on an encrypted job: delivered as initial chunk,
	//    rejected before key derivation, remainder (incl. terminator) never fed.
	{
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		std::string firstChunk =
			"=ybegin line=128 size=16 name=test.dat\r\n";
		decoder.DecodeBuffer(firstChunk.data(), static_cast<int>(firstChunk.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK(decoder.GetEof());
		BOOST_CHECK(!decoder.IsWireTerminated());
		BOOST_CHECK(decoder.GetDecryptedData().empty());
	}

	// 2. Invalid first chunk delivered separately; the delayed remainder with
	//    the real terminator is never fed (mirrors an undrained socket).
	{
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		std::string firstChunk =
			"4b376d5839704c32715238764e34775a99999999CORRUPT_LINE_ONE_DATA\r\n";
		decoder.DecodeBuffer(firstChunk.data(), static_cast<int>(firstChunk.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK(!decoder.IsWireTerminated());
	}

	// 3. Control: a fully delivered encrypted article (terminator consumed)
	//    reports the terminator as consumed, so ordinary connection reuse is
	//    unaffected.
	{
		YEncDecryptor decryptor("conformance-fixture-password");
		Decoder decoder;
		decoder.SetDecryptor(&decryptor);
		std::string wire =
			"4b376d5839704c32715238764e34775a99999999CORRUPT_LINE_ONE_DATA\r\n"
			"=ypart line=1 size=10\r\ndata\r\n=yend size=4\r\n.\r\n";
		decoder.DecodeBuffer(wire.data(), static_cast<int>(wire.size()));
		BOOST_CHECK_EQUAL(static_cast<int>(decoder.Check()), static_cast<int>(Decoder::dsAuthFailed));
		BOOST_CHECK(decoder.IsWireTerminated());
	}
}

BOOST_AUTO_TEST_SUITE_END()
