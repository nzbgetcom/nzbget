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

#include <boost/test/unit_test.hpp>
#include <cstring>
#include "Decoder.h"
#include "YEncDecryptor.h"
#include "YEncoder.h"
#include "Util.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "ArticleDownloader.h"
#include "YEncTestHelpers.h"

#include <argon2.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>

// Shared test helpers : one hex/yEnc helper set for all NNTP test TUs.
using namespace YEncTest;

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(MasterKeyDerivationAndCachingTest)
{
	YEncDecryptor decryptor("conformance-fixture-password");
	// Vector from argon2id.json: argon2id-01-basic
	// salt: 1a2b3c4d5e6f7890abcdef1234567890
	// expected: 7da24b7185cdf96dd32078b0fabfa34d141b282791cc641aa2d7afca3651d05f
	std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	BOOST_REQUIRE_EQUAL(salt.size(), 16);

	BOOST_CHECK(decryptor.EnsureMasterKey(salt.data()));
	BOOST_CHECK_EQUAL(decryptor.GetCachedSaltCount(), 1);

	std::string keyHex = BinToHex(decryptor.GetCachedMasterKey().data(), decryptor.GetCachedMasterKey().size());
	BOOST_CHECK_EQUAL(keyHex, "7da24b7185cdf96dd32078b0fabfa34d141b282791cc641aa2d7afca3651d05f");

	// Re-check caching: same salt shouldn't recalculate
	BOOST_CHECK(decryptor.EnsureMasterKey(salt.data()));
	BOOST_CHECK_EQUAL(decryptor.GetCachedSaltCount(), 1);
	std::string keyHex2 = BinToHex(decryptor.GetCachedMasterKey().data(), decryptor.GetCachedMasterKey().size());
	BOOST_CHECK_EQUAL(keyHex2, keyHex);
}

BOOST_AUTO_TEST_CASE(DeriveBodyNonceTest)
{
	// Nonce values themselves are asserted by the dynamic conformance suite
	// (DynamicNonceTweakTestVectorsTest); here only that derivation succeeds
	// and is deterministic for a fixed segment index.
	YEncDecryptor decryptor("conformance-fixture-password");
	std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	BOOST_REQUIRE(decryptor.EnsureMasterKey(salt.data()));

	uint8_t nonce1[24];
	BOOST_CHECK(decryptor.DeriveBodyNonce(1, nonce1));
	uint8_t nonce1Again[24];
	BOOST_CHECK(decryptor.DeriveBodyNonce(1, nonce1Again));
	BOOST_CHECK_EQUAL_COLLECTIONS(nonce1, nonce1 + 24, nonce1Again, nonce1Again + 24);
	uint8_t nonce2[24];
	BOOST_CHECK(decryptor.DeriveBodyNonce(2, nonce2));
	BOOST_CHECK(memcmp(nonce1, nonce2, 24) != 0);
}

BOOST_AUTO_TEST_CASE(DerivationFailsClosedWithoutMasterKeyTest)
{
	// Derivation must fail closed before any HMAC/copy work when no
	// master key is cached, leaving the output buffers untouched. The deeper
	// HMAC-failure branch (HMAC() returning nullptr) cannot be triggered
	// without a production hook; it is covered by static review: the return
	// pointer and mdLen are checked and md is zeroized on every exit.
	YEncDecryptor decryptor("conformance-fixture-password"); // EnsureMasterKey never called
	uint8_t nonce[24];
	memset(nonce, 0xAB, sizeof(nonce));
	BOOST_CHECK(!decryptor.DeriveBodyNonce(1, nonce));
	uint8_t expectedNonce[24];
	memset(expectedNonce, 0xAB, sizeof(expectedNonce));
	BOOST_CHECK_EQUAL_COLLECTIONS(nonce, nonce + 24, expectedNonce, expectedNonce + 24);

	uint8_t key[32];
	uint8_t tweak[8];
	memset(key, 0xAB, sizeof(key));
	memset(tweak, 0xAB, sizeof(tweak));
	BOOST_CHECK(!decryptor.DeriveControlKeyAndTweak(1, 1, key, tweak));
	uint8_t expectedKey[32];
	uint8_t expectedTweak[8];
	memset(expectedKey, 0xAB, sizeof(expectedKey));
	memset(expectedTweak, 0xAB, sizeof(expectedTweak));
	BOOST_CHECK_EQUAL_COLLECTIONS(key, key + 32, expectedKey, expectedKey + 32);
	BOOST_CHECK_EQUAL_COLLECTIONS(tweak, tweak + 8, expectedTweak, expectedTweak + 8);
}

BOOST_AUTO_TEST_CASE(ZeroOutputOnCorruptTagTest)
{
	YEncDecryptor decryptor("conformance-fixture-password");
	std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::vector<uint8_t> ciphertext = HexToBin("1136931a760f55f58a07bc9ce8329ede");
	std::vector<uint8_t> corruptTag = HexToBin("ffffffffffffffffffffffffffffffff");
	std::vector<uint8_t> aad = YEncDecryptor::BuildAad("file.bin", 1, 1, 16);

	std::vector<uint8_t> outPlaintext;
	YEncDecryptor::Status st = decryptor.AuthenticateAndDecrypt(
		ciphertext.data(), ciphertext.size(),
		salt.data(), corruptTag.data(), 1, aad.data(), aad.size(), outPlaintext);

	BOOST_CHECK_EQUAL(static_cast<int>(st), static_cast<int>(YEncDecryptor::Status::AuthFailed));
	BOOST_CHECK_EQUAL(outPlaintext.size(), 0); // Zero-Output Guarantee!
}

BOOST_AUTO_TEST_CASE(ParseYEncryptionTest)
{
	// Header field values and grammar rejections are asserted by the dynamic
	// conformance suites (DynamicBodyEncryptionTestVectorsTest and the
	// header_syntax category of DynamicMalformedInputsTestVectorsTest);
	// here only that a well-formed line parses and yields a non-zero index.
	std::string line = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=0cd77ce245a654463f90b945b1d22d5b\r\n";
	YEncDecryptor::YEncryptionHeader header;

	BOOST_CHECK(YEncDecryptor::ParseYEncryption(line.c_str(), line.length(), header));
	BOOST_CHECK_EQUAL(header.segmentIndex, 1U);
	BOOST_CHECK_EQUAL(header.salt.size(), 16U);
	BOOST_CHECK_EQUAL(header.tag.size(), 16U);
}

BOOST_AUTO_TEST_CASE(YEncryptionZeroOutputTest)
{
	// Ensure that on every failure condition, zero plaintext is returned and buffer is empty
	Decoder decoder;
	YEncDecryptor decryptor("conformance-fixture-password");
	decoder.SetDecryptor(&decryptor);

	// 1. Decoder check with no data
	auto st = decoder.Check();
	BOOST_CHECK_NE(static_cast<int>(st), static_cast<int>(Decoder::dsFinished));
	BOOST_CHECK_EQUAL(decoder.GetDecryptedData().size(), 0);

	// 2. Decoder with tampered ciphertext in encrypted wire article
	decoder.Clear();

	std::string password = "conformance-fixture-password";
	std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::vector<uint8_t> tamperedCt = HexToBin("6b0d1eb225f844920540fa3628eb9ff9d3174ea7e14a5ba7da1f0f6e970aed28");
	std::vector<uint8_t> tag = HexToBin("ed70d238067735a20783df5e094ccafa");
	uint32_t segmentIndex = 1;

	YEncDecryptor encDec(password);
	std::string line1Plain = "=ybegin line=128 size=32 name=tampered.dat";
	std::vector<uint8_t> wireLine1;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
		segmentIndex, 1, true, salt.data(), wireLine1
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string line2Plain = "=yencryption cipher=XChaCha20-Poly1305 salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 tag=ed70d238067735a20783df5e094ccafa";
	std::vector<uint8_t> wireLine2;
	BOOST_REQUIRE_EQUAL(static_cast<int>(encDec.EncryptControlLine(
		reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
		segmentIndex, 2, false, salt.data(), wireLine2
	)), static_cast<int>(YEncDecryptor::Status::Ok));

	std::string wireLine3 = SimpleYencEncode(tamperedCt);

	Crc32 crc;
	crc.Append(tamperedCt.data(), tamperedCt.size());
	uint32_t ctCrc = crc.Finish();
	std::ostringstream endOss;
	endOss << "=yend size=32 crc32=" << std::hex << std::setw(8) << std::setfill('0') << ctCrc;
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

	decoder.DecodeBuffer(fullWire.data(), fullWire.size());
	st = decoder.Check();
	BOOST_CHECK_EQUAL(static_cast<int>(st), static_cast<int>(Decoder::dsAuthFailed));
	BOOST_CHECK_EQUAL(decoder.GetDecryptedData().size(), 0);
}

BOOST_AUTO_TEST_CASE(YEncDecryptorAuthFailureZeroizationTest)
{
	YEncDecryptor decryptor("synthetic-auth-failure-fixture");
	uint8_t salt[16] = {0xaa, 0xbb, 0xcc, 0xdd, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xaa, 0xbb};
	uint8_t badTag[16] = {0}; // Invalid tag
	uint8_t ct[32];
	memset(ct, 0x42, sizeof(ct));

	std::vector<uint8_t> outPlaintext;
	// Pre-fill outPlaintext with dummy values
	outPlaintext.assign(64, 0xff);

	std::vector<uint8_t> aad = YEncDecryptor::BuildAad("file.bin", 1, 1, 32);
	auto st = decryptor.AuthenticateAndDecrypt(ct, sizeof(ct), salt, badTag, 1, aad.data(), aad.size(), outPlaintext);
	BOOST_CHECK(st == YEncDecryptor::Status::AuthFailed);
	// outPlaintext must be cleared and empty on AuthFailed
	BOOST_CHECK(outPlaintext.empty());
}

BOOST_AUTO_TEST_CASE(DecryptControlLineOutputsOnlyOnOkTest)
{
	// NG12-4: *outSalt/*outSegmentIndex are published only on Status::Ok.
	// A Line 1 that passes the bootstrap framing checks but fails key
	// derivation (empty password) must leave caller-sentinel outputs
	// untouched. This shape previously assigned the outputs before the
	// EnsureMasterKey failure return.
	YEncDecryptor emptyDecryptor("");
	const std::vector<uint8_t> salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");
	std::string wireHex = BinToHex(salt);
	wireHex += "00000001";
	wireHex += "3d3d";
	const std::vector<uint8_t> wire = HexToBin(wireHex);
	BOOST_REQUIRE_EQUAL(wire.size(), 22U);

	std::vector<uint8_t> plaintext;
	std::vector<uint8_t> outSalt = {0xAA, 0xBB};
	uint32_t outSegmentIndex = 0xDEADBEEF;
	auto st = emptyDecryptor.DecryptControlLine(
		wire.data(), wire.size(), 0, 1, true, plaintext, &outSalt, &outSegmentIndex);
	BOOST_CHECK(st == YEncDecryptor::Status::Error);
	BOOST_CHECK(plaintext.empty());
	BOOST_REQUIRE_EQUAL(outSalt.size(), 2U);
	BOOST_CHECK_EQUAL(static_cast<int>(outSalt[0]), 0xAA);
	BOOST_CHECK_EQUAL(static_cast<int>(outSalt[1]), 0xBB);
	BOOST_CHECK_EQUAL(outSegmentIndex, 0xDEADBEEFU);

	// The oversized-line local failure is fatal-classified (NG12-2).
	YEncDecryptor decryptor("conformance-fixture-password");
	std::vector<uint8_t> oversized(YEncDecryptor::MAX_LINE1_PLAINTEXT_SIZE + 1, 'a');
	st = decryptor.DecryptControlLine(
		oversized.data(), oversized.size(), 1, 2, false, plaintext);
	BOOST_CHECK(st == YEncDecryptor::Status::Error);
	BOOST_CHECK(decryptor.WasLastErrorFatal());

	// Post-EnsureMasterKey failure: a valid password derives the master
	// key, but a forbidden numeral byte (0x00) in the FF1 ciphertext makes
	// ByteToNumeral throw. The bootstrap salt/index must still not be
	// published (NG12-4), and the stale fatal flag from the oversized
	// call above must be reset by this non-fatal wire failure (NG12-2).
	{
		YEncDecryptor wireDecryptor("conformance-fixture-password");
		// Prime m_lastErrorFatal with a fatal (oversized) failure.
		std::vector<uint8_t> primePt;
		BOOST_CHECK(wireDecryptor.DecryptControlLine(
			oversized.data(), oversized.size(), 1, 2, false, primePt) == YEncDecryptor::Status::Error);
		BOOST_REQUIRE(wireDecryptor.WasLastErrorFatal());

		std::vector<uint8_t> wire2 = salt;
		wire2.push_back(0x00);
		wire2.push_back(0x00);
		wire2.push_back(0x00);
		wire2.push_back(0x01); // segmentIndex = 1
		wire2.push_back(0x00); // forbidden FF1 ciphertext byte
		wire2.push_back(0x01);
		BOOST_REQUIRE_EQUAL(wire2.size(), 22U);

		std::vector<uint8_t> outSalt2 = {0xAA, 0xBB};
		uint32_t outSegmentIndex2 = 0xDEADBEEF;
		std::vector<uint8_t> plaintext2;
		auto st2 = wireDecryptor.DecryptControlLine(
			wire2.data(), wire2.size(), 0, 1, true, plaintext2, &outSalt2, &outSegmentIndex2);
		BOOST_CHECK(st2 == YEncDecryptor::Status::Error);
		BOOST_CHECK(plaintext2.empty());
		BOOST_REQUIRE_EQUAL(outSalt2.size(), 2U);
		BOOST_CHECK_EQUAL(static_cast<int>(outSalt2[0]), 0xAA);
		BOOST_CHECK_EQUAL(static_cast<int>(outSalt2[1]), 0xBB);
		BOOST_CHECK_EQUAL(outSegmentIndex2, 0xDEADBEEFU);
		BOOST_CHECK(!wireDecryptor.WasLastErrorFatal());
	}

	// Same stale-flag reset for AuthenticateAndDecrypt: a fatal error
	// followed by a non-fatal AuthFailed must clear the flag.
	{
		YEncDecryptor authDecryptor("");
		const std::vector<uint8_t> tag = HexToBin("0cd77ce245a654463f90b945b1d22d5b");
		const std::vector<uint8_t> aad = YEncDecryptor::BuildAad("file.bin", 1, 1, 1);
		const uint8_t ct[1] = {0};
		std::vector<uint8_t> out;
		BOOST_CHECK(authDecryptor.AuthenticateAndDecrypt(ct, sizeof(ct), salt.data(), tag.data(), 1,
			aad.data(), aad.size(), out) == YEncDecryptor::Status::Error);
		BOOST_REQUIRE(authDecryptor.WasLastErrorFatal());

		YEncDecryptor authDecryptor2("conformance-fixture-password");
		// Prime fatal via empty-password decryptor is a different instance;
		// instead prime this instance with an oversized control line.
		std::vector<uint8_t> primePt;
		BOOST_CHECK(authDecryptor2.DecryptControlLine(
			oversized.data(), oversized.size(), 1, 2, false, primePt) == YEncDecryptor::Status::Error);
		BOOST_REQUIRE(authDecryptor2.WasLastErrorFatal());
		const std::vector<uint8_t> badTag(16, 0xFF);
		const std::vector<uint8_t> ctVec = HexToBin("1136931a760f55f58a07bc9ce8329ede");
		BOOST_CHECK(authDecryptor2.AuthenticateAndDecrypt(ctVec.data(), ctVec.size(), salt.data(), badTag.data(), 1,
			aad.data(), aad.size(), out) == YEncDecryptor::Status::AuthFailed);
		BOOST_CHECK(!authDecryptor2.WasLastErrorFatal());
		BOOST_CHECK(out.empty());
	}
}

// NZBGET-TEST-05 decision: deleted NZBGet's fixture copy
// tests/testdata/test-vectors/index_allocation.json (downloader-only client,
// uploader allocation fixture not applicable). This test directly asserts
// downloader rejection of forbidden segment indices (0x0A/0x0D bytes).
BOOST_AUTO_TEST_CASE(ForbiddenSegmentIndexByteRejectTest)
{
	// Index framing rule (Body Std v1.2 §8): any uint32_be(segmentIndex) byte
	// 0x0A/0x0D would split Line 1 on the wire. Bootstrap extraction must reject (Status::Error).
	// Wire line 1: [16B salt][4B BE index][2B FF1 ct] — same salt as canonical vectors.
	struct Case
	{
		uint32_t index;
		const char* indexHex;
	};
	const Case cases[] = {
		{10, "0000000a"},
		{13, "0000000d"},
		{266, "0000010a"},
		{269, "0000010d"},
	};
	for (const auto& testCase : cases)
	{
		std::string line1 = "4b376d5839704c32715238764e34775a";
		line1 += testCase.indexHex;
		line1 += "3d3d";
		std::vector<uint8_t> wire = HexToBin(line1);
		BOOST_REQUIRE_EQUAL(wire.size(), 22U);

		YEncDecryptor decryptor("conformance-fixture-password");
		std::vector<uint8_t> plaintext;
		std::vector<uint8_t> salt;
		uint32_t extractedIndex = 0;
		auto st = decryptor.DecryptControlLine(
			wire.data(), wire.size(), 0, 1, true, plaintext, &salt, &extractedIndex);
		BOOST_CHECK(st == YEncDecryptor::Status::Error);
		BOOST_CHECK(plaintext.empty());
		BOOST_CHECK(salt.empty());
		BOOST_CHECK_EQUAL(extractedIndex, 0U);

		// The same rule applies to the =yencryption index token (body-only header).
		std::string header = "=yencryption cipher=XChaCha20-Poly1305 "
			"salt=1a2b3c4d5e6f7890abcdef1234567890 index=";
		header += testCase.indexHex;
		header += " tag=0cd77ce245a654463f90b945b1d22d5b";
		YEncDecryptor::YEncryptionHeader parsed;
		BOOST_CHECK(!YEncDecryptor::ParseYEncryption(header.data(), header.size(), parsed));
	}

	// Control: a safe index (11) with the same salt decrypts end-to-end.
	// Build Line 1 with the real encryptor so FF1 runs on a genuine wire.
	{
		const std::vector<uint8_t> controlSalt = HexToBin("4b376d5839704c32715238764e34775a");
		const std::string line1Plain = "=ybegin line=128 size=18 name=file.bin";
		YEncDecryptor encryptor("conformance-fixture-password");
		std::vector<uint8_t> safeWire;
		BOOST_REQUIRE_EQUAL(static_cast<int>(encryptor.EncryptControlLine(
			reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
			11, 1, true, controlSalt.data(), safeWire)), static_cast<int>(YEncDecryptor::Status::Ok));

		YEncDecryptor decryptor("conformance-fixture-password");
		std::vector<uint8_t> plaintext;
		std::vector<uint8_t> salt;
		uint32_t extractedIndex = 0;
		auto st = decryptor.DecryptControlLine(
			safeWire.data(), safeWire.size(), 0, 1, true, plaintext, &salt, &extractedIndex);
		BOOST_CHECK(st == YEncDecryptor::Status::Ok);
		BOOST_CHECK_EQUAL(extractedIndex, 11U);
		BOOST_CHECK_EQUAL(std::string(plaintext.begin(), plaintext.end()), line1Plain);
		BOOST_CHECK_EQUAL(BinToHex(salt), BinToHex(controlSalt));
	}
}

BOOST_AUTO_TEST_CASE(JobOwnedKeyCacheSharingTest)
{
	// Job-owned Argon2id master-key cache:
	// 1. Sharing across separate YEncDecryptor instances for the same job
	//    (single-entry: one salt per upload per the standard).
	// 2. Cache clearing on password change or job teardown.
	YEncKeyCache jobCache;
	uint8_t salt1[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

	// Worker 1: derives key for salt1 and stores into jobCache
	YEncDecryptor worker1("synthetic-cache-fixture", &jobCache);
	BOOST_CHECK(worker1.EnsureMasterKey(salt1));
	BOOST_CHECK_EQUAL(jobCache.Size(), 1U);

	// Worker 2 (separate thread/instance on same job): fetches salt1 key instantly without running Argon2id
	YEncDecryptor worker2("synthetic-cache-fixture", &jobCache);
	BOOST_CHECK(worker2.EnsureMasterKey(salt1));
	BOOST_CHECK_EQUAL(jobCache.Size(), 1U);
	BOOST_CHECK_EQUAL_COLLECTIONS(
		worker1.GetCachedMasterKey().begin(), worker1.GetCachedMasterKey().end(),
		worker2.GetCachedMasterKey().begin(), worker2.GetCachedMasterKey().end());

	// Clearing cache (e.g. password change or job teardown) zeroizes and drops the entry;
	// the next derivation runs the KDF again and yields the same key.
	jobCache.Clear();
	BOOST_CHECK_EQUAL(jobCache.Size(), 0U);
	BOOST_CHECK(worker2.EnsureMasterKey(salt1));
	BOOST_CHECK_EQUAL_COLLECTIONS(
		worker1.GetCachedMasterKey().begin(), worker1.GetCachedMasterKey().end(),
		worker2.GetCachedMasterKey().begin(), worker2.GetCachedMasterKey().end());
}

BOOST_AUTO_TEST_CASE(ConcurrentEnsureMasterKeyDerivesOnceTest)
{
	// Single-flight: the lock is held across the derive callback, so N
	// parallel callers for the same salt run the (expensive) KDF exactly once.
	YEncKeyCache cache;
	uint8_t salt[16] = {1};
	std::atomic<int> runs{0};
	std::atomic<int> successes{0};
	std::vector<std::thread> workers;
	for (int i = 0; i < 8; ++i)
	{
		workers.emplace_back([&]()
		{
			uint8_t key[32];
			if (cache.GetOrDeriveKey(salt, "pass" "word", key, [&](uint8_t* output)
				{
					++runs;
					// Costly derivation stand-in; serialized by the cache lock.
					return argon2id_hash_raw(1, 4096, 1, "pass" "word", 8, salt, 16, output, 32) == ARGON2_OK;
				}))
			{
				++successes;
			}
		});
	}
	for (auto& worker : workers) worker.join();
	BOOST_CHECK_EQUAL(runs, 1);
	BOOST_CHECK_EQUAL(successes, 8);
}

BOOST_AUTO_TEST_CASE(PasswordEditAfterQueueUsesNewKeyTest)
{
	YEncKeyCache cache;
	uint8_t salt[16] = {1};
	YEncDecryptor oldWorker("synthetic-old-fixture", &cache);
	YEncDecryptor newWorker("synthetic-new-fixture", &cache);
	YEncDecryptor independent("synthetic-new-fixture");
	BOOST_REQUIRE(oldWorker.EnsureMasterKey(salt));
	// The cached entry is keyed by salt + password hash, so a password edit
	// cannot reuse the old key (ArticleDownloader rebuilds the decryptor).
	BOOST_REQUIRE(newWorker.EnsureMasterKey(salt));
	BOOST_REQUIRE(independent.EnsureMasterKey(salt));
	BOOST_CHECK(newWorker.GetCachedMasterKey() != oldWorker.GetCachedMasterKey());
	BOOST_CHECK(newWorker.GetCachedMasterKey() == independent.GetCachedMasterKey());
}

BOOST_AUTO_TEST_SUITE_END()
