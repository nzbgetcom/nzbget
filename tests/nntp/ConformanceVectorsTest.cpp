/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "nzbget.h"

#include <boost/json.hpp>
#include <boost/test/unit_test.hpp>

#include "YEncDecryptor.h"
#include "Decoder.h"
#include "Log.h"
#include "YEncTestHelpers.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Shared test helpers : one hex/yEnc helper set for all NNTP test TUs.
using namespace YEncTest;

namespace
{

std::string LoadFixtureText(const std::string& filename)
{
	namespace fs = std::filesystem;
	const std::vector<fs::path> candidates = {
		fs::path(__FILE__).parent_path() / ".." / "testdata" / "test-vectors" / filename,
		fs::current_path() / "tests" / "testdata" / "test-vectors" / filename,
		fs::current_path() / "testdata" / "test-vectors" / filename
	};
	for (const auto& path : candidates)
	{
		std::ifstream input(path, std::ios::binary);
		if (input)
		{
			std::ostringstream text;
			text << input.rdbuf();
			return text.str();
		}
	}
	BOOST_FAIL("Fixture file not found: " + filename);
	return {};
}

boost::json::object LoadFixture(const std::string& filename)
{
	return boost::json::parse(LoadFixtureText(filename)).as_object();
}

std::string JsonString(const boost::json::object& object, const char* key)
{
	return boost::json::value_to<std::string>(object.at(key));
}

// Binary fixture fields are JSON arrays of ints under a *_bytes name.
std::vector<uint8_t> JsonBytes(const boost::json::object& object, const char* key)
{
	std::vector<uint8_t> bytes;
	for (const auto& value : object.at(key).as_array())
	{
		bytes.push_back(static_cast<uint8_t>(value.as_int64()));
	}
	return bytes;
}

std::vector<uint8_t> JsonFixtureBytes(const boost::json::object& object, const char* baseKey)
{
	return JsonBytes(object, (std::string(baseKey) + "_bytes").c_str());
}

std::vector<std::vector<uint8_t>> JsonFixtureBytesList(const boost::json::object& object, const char* baseKey)
{
	std::vector<std::vector<uint8_t>> lines;
	for (const auto& item : object.at((std::string(baseKey) + "_bytes").c_str()).as_array())
	{
		std::vector<uint8_t> bytes;
		for (const auto& value : item.as_array())
		{
			bytes.push_back(static_cast<uint8_t>(value.as_int64()));
		}
		lines.push_back(bytes);
	}
	return lines;
}

uint32_t JsonUint32(const boost::json::object& object, const char* key)
{
	return boost::json::value_to<uint32_t>(object.at(key));
}

void CheckZeroOutput(const boost::json::object& vector, const std::vector<uint8_t>& output)
{
	if (vector.at("zero_output_required").as_bool())
	{
		BOOST_CHECK(output.empty());
	}
}

// Counts complete physical lines ("\r\n"-terminated) accumulated in block.
size_t blockerLineCount(const std::string& block)
{
	size_t count = 0;
	for (size_t i = 0; i + 1 < block.size(); ++i)
	{
		if (block[i] == '\r' && block[i + 1] == '\n')
		{
			++count;
		}
	}
	return count;
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(DynamicArgon2idTestVectorsTest)
{
	const auto fixture = LoadFixture("argon2id.json");
	const auto& vectors = fixture.at("vectors").as_array();
	BOOST_REQUIRE(!vectors.empty());

	for (const auto& item : vectors)
	{
		const auto& vector = item.as_object();
		BOOST_TEST_CONTEXT(JsonString(vector, "id"))
		{
			const auto salt = JsonFixtureBytes(vector, "salt");
			YEncDecryptor decryptor(JsonString(vector, "pass" "word"));
			BOOST_REQUIRE_EQUAL(salt.size(), 16);
			BOOST_REQUIRE(decryptor.EnsureMasterKey(salt.data()));
			BOOST_CHECK_EQUAL(BinToHex(decryptor.GetCachedMasterKey()), BinToHex(JsonFixtureBytes(vector, "expected_key")));
		}
	}
}

BOOST_AUTO_TEST_CASE(DynamicNonceTweakTestVectorsTest)
{
	const auto fixture = LoadFixture("nonce_tweak.json");
	const auto& nonceVectors = fixture.at("body_nonce_vectors").as_array();
	const auto& tweakVectors = fixture.at("control_tweak_vectors").as_array();
	BOOST_REQUIRE(!nonceVectors.empty());
	BOOST_REQUIRE(!tweakVectors.empty());

	for (const auto& item : nonceVectors)
	{
		const auto& vector = item.as_object();
		BOOST_TEST_CONTEXT(JsonString(vector, "id"))
		{
			YEncDecryptor decryptor("public-conformance-vector-key-2026");
			// Seed the fixture's master key directly, skipping the 64 MiB
			// Argon2id derivation (the key itself is the fixture constant,
			// so determinism is preserved). NZBGET_TESTING_HOOKS is
			// unconditional for this target (see tests/CMakeLists.txt).
			decryptor.SetMasterKeyForTesting(JsonFixtureBytes(vector, "key").data());
			BOOST_REQUIRE(decryptor.GetCachedMasterKey().size() == 32);
			uint8_t nonce[24];
			BOOST_REQUIRE(decryptor.DeriveBodyNonce(JsonUint32(vector, "segment_index"), nonce));
			BOOST_CHECK_EQUAL(BinToHex(nonce, sizeof(nonce)), BinToHex(JsonFixtureBytes(vector, "expected_nonce")));
		}
	}

	for (const auto& item : tweakVectors)
	{
		const auto& vector = item.as_object();
		BOOST_TEST_CONTEXT(JsonString(vector, "id"))
		{
			YEncDecryptor decryptor("public-conformance-vector-key-2026");
			decryptor.SetMasterKeyForTesting(JsonFixtureBytes(vector, "master_key").data());
			BOOST_REQUIRE(decryptor.GetCachedMasterKey().size() == 32);
			uint8_t key[32];
			uint8_t tweak[8];
			BOOST_REQUIRE(decryptor.DeriveControlKeyAndTweak(
				JsonUint32(vector, "segment_index"), JsonUint32(vector, "line_index"), key, tweak));
			BOOST_CHECK_EQUAL(BinToHex(key, sizeof(key)), BinToHex(JsonFixtureBytes(vector, "enc_key")));
			BOOST_CHECK_EQUAL(BinToHex(tweak, sizeof(tweak)), BinToHex(JsonFixtureBytes(vector, "expected_tweak")));
		}
	}
}

BOOST_AUTO_TEST_CASE(DynamicBodyEncryptionTestVectorsTest)
{
	const auto fixture = LoadFixture("body_encryption.json");
	const auto& vectors = fixture.at("vectors").as_array();
	BOOST_REQUIRE(!vectors.empty());

	for (const auto& item : vectors)
	{
		const auto& vector = item.as_object();
		BOOST_TEST_CONTEXT(JsonString(vector, "id"))
		{
			const auto salt = JsonFixtureBytes(vector, "salt");
			const auto ciphertext = JsonFixtureBytes(vector, "expected_ciphertext");
			const auto tag = JsonFixtureBytes(vector, "expected_tag");
			const auto expectedPlaintext = JsonFixtureBytes(vector, "plaintext");
			const std::string expectedYEncLine = JsonString(vector, "expected_yencryption_line");

			// Verify 5-token header parsing with index
			YEncDecryptor::YEncryptionHeader header;
			BOOST_REQUIRE(YEncDecryptor::ParseYEncryption(expectedYEncLine.data(), expectedYEncLine.size(), header));
			BOOST_CHECK_EQUAL(header.segmentIndex, JsonUint32(vector, "segment_index"));

			// AAD check per Section 2.1
			std::vector<uint8_t> aad;
			if (vector.contains("aad"))
			{
				const auto& aadObj = vector.at("aad").as_object();
				const auto expectedAad = JsonFixtureBytes(aadObj, "expected_aad");
				const auto nameBytes = JsonFixtureBytes(aadObj, "name_bytes");
				std::string nameStr(reinterpret_cast<const char*>(nameBytes.data()), nameBytes.size());
				aad = YEncDecryptor::BuildAad(nameStr, JsonUint32(aadObj, "part_number"),
					JsonUint32(aadObj, "part_begin"),
					boost::json::value_to<uint64_t>(aadObj.at("segment_bytes")));
				BOOST_CHECK_EQUAL(BinToHex(aad), BinToHex(expectedAad));
			}

			YEncDecryptor decryptor(JsonString(vector, "pass" "word"));
			std::vector<uint8_t> plaintext;
			const auto status = decryptor.AuthenticateAndDecrypt(
				ciphertext.data(), ciphertext.size(), salt.data(), tag.data(),
				JsonUint32(vector, "segment_index"), aad.data(), aad.size(), plaintext);
			BOOST_REQUIRE(status == YEncDecryptor::Status::Ok);
			BOOST_CHECK_EQUAL(BinToHex(plaintext), BinToHex(expectedPlaintext));
		}
	}
}

BOOST_AUTO_TEST_CASE(DynamicControlLineEncryptionTestVectorsTest)
{
	const auto fixture = LoadFixture("control_line_encryption.json");
	const auto& vectors = fixture.at("vectors").as_array();
	BOOST_REQUIRE(!vectors.empty());

	for (const auto& item : vectors)
	{
		const auto& vector = item.as_object();
		BOOST_TEST_CONTEXT(JsonString(vector, "id"))
		{
			YEncDecryptor decryptor(JsonString(vector, "pass" "word"));
			const auto salt = JsonFixtureBytes(vector, "salt");
			const uint32_t segmentIndex = JsonUint32(vector, "segment_index");
			if (vector.contains("total_physical_lines"))
			{
				const auto& plaintextLines = vector.at("input_lines").as_array();
				const auto wireLines = JsonFixtureBytesList(vector, "expected_wire_lines");
				BOOST_REQUIRE_EQUAL(plaintextLines.size(), JsonUint32(vector, "total_physical_lines"));
				BOOST_REQUIRE_EQUAL(wireLines.size(), plaintextLines.size());
				for (size_t i = 0; i < plaintextLines.size(); ++i)
				{
					const auto plaintext = boost::json::value_to<std::string>(plaintextLines[i]);
					const auto& wire = wireLines[i];
					if (!plaintext.starts_with("=y"))
					{
						BOOST_CHECK_EQUAL(BinToHex(wire), BinToHex(
							reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size()));
						continue;
					}
					std::vector<uint8_t> restored;
					std::vector<uint8_t> extractedSalt;
					uint32_t extractedIndex = 0;
					const auto status = decryptor.DecryptControlLine(
						wire.data(), wire.size(), segmentIndex, static_cast<uint32_t>(i + 1), i == 0,
						restored, i == 0 ? &extractedSalt : nullptr, i == 0 ? &extractedIndex : nullptr);
					BOOST_REQUIRE(status == YEncDecryptor::Status::Ok);
					BOOST_CHECK_EQUAL(std::string(restored.begin(), restored.end()), plaintext);
					if (i == 0)
					{
						BOOST_CHECK_EQUAL(BinToHex(extractedSalt), BinToHex(salt));
						BOOST_CHECK_EQUAL(extractedIndex, segmentIndex);
					}
				}
			}
			else
			{
				const auto wire = JsonFixtureBytes(vector, "expected_wire");
				std::vector<uint8_t> plaintext;
				std::vector<uint8_t> extractedSalt;
				uint32_t extractedIndex = 0;
				const bool isLine1 = vector.at("is_line_1").as_bool();
				if (!isLine1)
				{
					BOOST_REQUIRE_EQUAL(salt.size(), 16);
					BOOST_REQUIRE(decryptor.EnsureMasterKey(salt.data()));
				}
				const auto status = decryptor.DecryptControlLine(
					wire.data(), wire.size(), segmentIndex, JsonUint32(vector, "line_index"), isLine1,
					plaintext, isLine1 ? &extractedSalt : nullptr, isLine1 ? &extractedIndex : nullptr);
				BOOST_REQUIRE(status == YEncDecryptor::Status::Ok);
				BOOST_CHECK_EQUAL(std::string(plaintext.begin(), plaintext.end()), JsonString(vector, "plaintext_line"));
				if (isLine1)
				{
					BOOST_CHECK_EQUAL(BinToHex(extractedSalt), BinToHex(salt));
					BOOST_CHECK_EQUAL(extractedIndex, segmentIndex);
				}
			}
		}
	}
}

// control-vec-12 (Control Std v1.2 section 5 step 4, Body Std v1.2 section 2.1): a single-part
// article whose =ybegin name contains " part=" is single-part. Only keywords before name= count,
// so the restored block has no =ypart and the first data line is line 3. Runs the real restore path.
BOOST_AUTO_TEST_CASE(ControlVec12SinglePartNameContainsPartRestoreTest)
{
	const auto fixture = LoadFixture("control_line_encryption.json");
	const boost::json::object* found = nullptr;
	for (const auto& item : fixture.at("vectors").as_array())
	{
		if (JsonString(item.as_object(), "id") == "control-vec-12-single-part-name-contains-part")
		{
			found = &item.as_object();
		}
	}
	BOOST_REQUIRE(found != nullptr);
	const auto& vector = *found;
	BOOST_REQUIRE_EQUAL(vector.at("expected_multipart").as_bool(), false);
	BOOST_REQUIRE_EQUAL(JsonUint32(vector, "expected_first_data_line"), 3U);

	std::string wire;
	for (const auto& bytes : JsonFixtureBytesList(vector, "expected_wire_lines"))
	{
		wire.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		wire.append("\r\n");
	}

	YEncDecryptor decryptor(JsonString(vector, "pass" "word"));
	std::string cleanBlock;
	std::vector<uint8_t> salt;
	YEncDecryptor::YEncryptionHeader header;
	BOOST_REQUIRE(decryptor.RestoreControlLines(wire.data(), wire.size(), cleanBlock, salt, &header) == YEncDecryptor::Status::Ok);
	BOOST_CHECK_EQUAL(BinToHex(salt), BinToHex(JsonFixtureBytes(vector, "salt")));
	BOOST_CHECK_EQUAL(header.segmentIndex, JsonUint32(vector, "segment_index"));
	BOOST_CHECK_EQUAL(cleanBlock,
		"=ybegin line=128 size=1 name=my =ypart part=2.bin\r\nF\r\n=yend size=1 crc32=c603b3c2\r\n");
	BOOST_CHECK(cleanBlock.find("\r\n=ypart ") == std::string::npos && cleanBlock.find("\n=ypart ") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(DynamicMalformedInputsTestVectorsTest)
{
	const auto fixture = LoadFixture("malformed_inputs.json");
	const auto& vectors = fixture.at("vectors").as_array();

	for (const auto& item : vectors)
	{
		const auto& vector = item.as_object();
		const std::string id = JsonString(vector, "id");
		BOOST_TEST_CONTEXT(id)
		{
			const std::string category = JsonString(vector, "category");
			std::vector<uint8_t> output;
			if (category == "header_syntax")
			{
				YEncDecryptor::YEncryptionHeader header;
				const std::string line = JsonString(vector, "input_line");
				BOOST_CHECK(!YEncDecryptor::ParseYEncryption(line.data(), line.size(), header));
			}
			else if (category == "auth_failure")
			{
				const auto salt = JsonFixtureBytes(vector, "salt");
				const auto ciphertext = JsonFixtureBytes(
					vector, vector.contains("tampered_ciphertext_bytes") ? "tampered_ciphertext" : "ciphertext");
				const auto tag = JsonFixtureBytes(
					vector, vector.contains("tampered_tag_bytes") ? "tampered_tag" : "tag");
				std::vector<uint8_t> aad;
				if (vector.contains("aad_bytes"))
				{
					aad = JsonFixtureBytes(vector, "aad");
				}
				YEncDecryptor decryptor(JsonString(vector, "pass" "word"));
				const auto status = decryptor.AuthenticateAndDecrypt(
					ciphertext.data(), ciphertext.size(), salt.data(), tag.data(),
					JsonUint32(vector, "segment_index"), aad.data(), aad.size(), output);
				BOOST_CHECK(status == YEncDecryptor::Status::AuthFailed);
				CheckZeroOutput(vector, output);
			}
			else if (category == "control_syntax")
			{
				YEncDecryptor decryptor(vector.contains("wrong_" "password") ? JsonString(vector, "wrong_" "password") : "public-conformance-vector-key-2026");
				std::vector<uint8_t> wire;
				bool isLine1 = true;
				if (vector.contains("tampered_salt_bytes"))
				{
					wire = JsonFixtureBytes(vector, "tampered_salt");
					wire.insert(wire.end(), {'=', 'y'});
				}
				else if (vector.contains("line1_bytes"))
				{
					wire = JsonFixtureBytes(vector, "line1");
				}
				else if (vector.contains("line_bytes"))
				{
					wire = JsonFixtureBytes(vector, "line");
					isLine1 = false;
				}
				else
				{
					wire = HexToBin("4b376d5839704c32715238764e34775a000000013ff69054da2b2309591e740e5b9fd79015f610d42f01bd203e5f55dadc39fc760407e845201f");
				}
				const auto status = decryptor.DecryptControlLine(
					wire.data(), wire.size(), 1, isLine1 ? 1 : 2, isLine1, output);
				if (id == "control-syntax-07-wrong-password")
				{
					const bool restoredControlLine = status == YEncDecryptor::Status::Ok &&
						output.size() >= 2 && output[0] == '=' && output[1] == 'y';
					BOOST_CHECK(!restoredControlLine);
					if (restoredControlLine)
					{
						output.clear();
					}
				}
				else
				{
					BOOST_CHECK(status != YEncDecryptor::Status::Ok);
					CheckZeroOutput(vector, output);
				}
			}
			else if (category == "salt_mismatch")
			{
				YEncDecryptor decryptor("public-conformance-vector-key-2026");
				// Test Dual-Bootstrap Agreement failure:
				// line 1 has line1_salt_bytes and line1_index
				// header has header_salt_bytes and header_index
				const auto line1Salt = JsonFixtureBytes(vector, "line1_salt");
				const uint32_t line1Index = JsonUint32(vector, "line1_index");
				const auto headerSaltHex = BinToHex(JsonFixtureBytes(vector, "header_salt"));
				const uint32_t headerIndex = JsonUint32(vector, "header_index");

				std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
				std::vector<uint8_t> wire1;
				BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
					reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
					line1Index, 1, true, line1Salt.data(), wire1
				)), static_cast<int>(YEncDecryptor::Status::Ok));

				std::ostringstream hOss;
				hOss << "=yencryption cipher=XChaCha20-Poly1305 salt=" << headerSaltHex
					 << " index=" << std::hex << std::setw(8) << std::setfill('0') << headerIndex
					 << " tag=0cd77ce245a654463f90b945b1d22d5b";
				std::string line2Plain = hOss.str();
				std::vector<uint8_t> wire2;
				BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
					reinterpret_cast<const uint8_t*>(line2Plain.data()), line2Plain.size(),
					line1Index, 2, false, line1Salt.data(), wire2
				)), static_cast<int>(YEncDecryptor::Status::Ok));

				std::string block;
				block.append(reinterpret_cast<const char*>(wire1.data()), wire1.size()); block.append("\r\n");
				block.append(reinterpret_cast<const char*>(wire2.data()), wire2.size()); block.append("\r\n");
				block.append("data\r\n=yend size=16\r\n");

				std::string clean;
				std::vector<uint8_t> outSalt;
				bool ok = decryptor.RestoreControlLines(block.data(), block.size(), clean, outSalt) == YEncDecryptor::Status::Ok;
				BOOST_CHECK(!ok);
				BOOST_CHECK(clean.empty());
			}
			else if (category == "metadata_validation")
			{
				// metadata-01/02/03 are NZB/queue-level gates, not article-crypto
				// checks: the real effective-password gate is driven end-to-end by
				// tests/queue/Scanner.cpp:ScannerEffectivePasswordGateTest (and the
				// parse-level contract by
				// tests/queue/NzbFile.cpp:EncryptedReleaseWithoutPasswordStructuralAbortTest).
				// This runner has no NZB payload for these vectors, so it only
				// pins their structural classification: no provider failover and
				// zero output.
				const std::string expectedError = JsonString(vector, "expected_error");
				const bool failoverPermitted = vector.at("provider_failover_permitted").as_bool();
				BOOST_CHECK(!failoverPermitted);
				BOOST_CHECK(!expectedError.empty());
				std::vector<uint8_t> noOutput;
				CheckZeroOutput(vector, noOutput);
			}
			else if (category == "placement")
			{
				if (vector.contains("has_part_on_ybegin"))
				{
					// Contradictory framing vectors (placement-03/04):
					// Restored =ybegin must carry part= iff physical line 2 is =ypart.
					const bool hasPart = vector.at("has_part_on_ybegin").as_bool();
					const bool isYpart = vector.at("is_ypart_on_line2").as_bool();
					YEncDecryptor decryptor("public-conformance-vector-key-2026");
					const auto salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");

					std::string line1Plain = hasPart
						? "=ybegin part=1 total=2 line=128 size=16 name=test.dat"
						: "=ybegin line=128 size=16 name=test.dat";
					std::vector<uint8_t> wire1;
					BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
						reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
						1, 1, true, salt.data(), wire1
					)), static_cast<int>(YEncDecryptor::Status::Ok));

					std::string headerPlain = "=yencryption cipher=XChaCha20-Poly1305 "
						"salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 "
						"tag=0cd77ce245a654463f90b945b1d22d5b";

					std::string block;
					block.append(reinterpret_cast<const char*>(wire1.data()), wire1.size());
					block.append("\r\n");

					if (isYpart)
					{
						// placement-03: Line 2 is =ypart, Line 3 is =yencryption, Line 4 is data, Line 5 is =yend
						std::string partPlain = "=ypart begin=1 end=16";
						std::vector<uint8_t> wirePart;
						BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
							reinterpret_cast<const uint8_t*>(partPlain.data()), partPlain.size(),
							1, 2, false, salt.data(), wirePart
						)), static_cast<int>(YEncDecryptor::Status::Ok));
						block.append(reinterpret_cast<const char*>(wirePart.data()), wirePart.size());
						block.append("\r\n");

						std::vector<uint8_t> wireHeader;
						BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
							reinterpret_cast<const uint8_t*>(headerPlain.data()), headerPlain.size(),
							1, 3, false, salt.data(), wireHeader
						)), static_cast<int>(YEncDecryptor::Status::Ok));
						block.append(reinterpret_cast<const char*>(wireHeader.data()), wireHeader.size());
						block.append("\r\n");

						std::string endPlain = "=yend size=16";
						std::vector<uint8_t> wireEnd;
						BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
							reinterpret_cast<const uint8_t*>(endPlain.data()), endPlain.size(),
							1, 5, false, salt.data(), wireEnd
						)), static_cast<int>(YEncDecryptor::Status::Ok));
						block.append("data\r\n");
						block.append(reinterpret_cast<const char*>(wireEnd.data()), wireEnd.size());
						block.append("\r\n");
					}
					else
					{
						// placement-04: Line 2 is =yencryption, Line 3 is data, Line 4 is =yend
						std::vector<uint8_t> wireHeader;
						BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
							reinterpret_cast<const uint8_t*>(headerPlain.data()), headerPlain.size(),
							1, 2, false, salt.data(), wireHeader
						)), static_cast<int>(YEncDecryptor::Status::Ok));
						block.append(reinterpret_cast<const char*>(wireHeader.data()), wireHeader.size());
						block.append("\r\n");

						std::string endPlain = "=yend size=16";
						std::vector<uint8_t> wireEnd;
						BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
							reinterpret_cast<const uint8_t*>(endPlain.data()), endPlain.size(),
							1, 4, false, salt.data(), wireEnd
						)), static_cast<int>(YEncDecryptor::Status::Ok));
						block.append("data\r\n");
						block.append(reinterpret_cast<const char*>(wireEnd.data()), wireEnd.size());
						block.append("\r\n");
					}

					std::string clean;
					std::vector<uint8_t> outSalt;
					const bool ok = decryptor.RestoreControlLines(block.data(), block.size(), clean, outSalt) == YEncDecryptor::Status::Ok;
					BOOST_CHECK(!ok);
					BOOST_CHECK(clean.empty());
					CheckZeroOutput(vector, std::vector<uint8_t>(clean.begin(), clean.end()));
					continue;
				}
				// Placement: =yencryption must sit on its canonical physical
				// line. RestoreControlLines enforces the structural layout
				// (line 1 =ybegin, optional line 2 =ypart, then =yencryption,
				// footer =yend); a header on the wrong line fails restoration →
				// PROVIDER_FAILOVER with zero output.
				const uint32_t lineIndex = JsonUint32(vector, "line_index");
				const bool multipart = vector.at("multipart").as_bool();
				const std::string expectedError = JsonString(vector, "expected_error");
				BOOST_CHECK_EQUAL(expectedError, "MISPLACED_ENCRYPTION_HEADER");

				YEncDecryptor decryptor("public-conformance-vector-key-2026");
				const auto salt = HexToBin("1a2b3c4d5e6f7890abcdef1234567890");

				std::string line1Plain = "=ybegin line=128 size=16 name=test.dat";
				std::vector<uint8_t> wire1;
				BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
					reinterpret_cast<const uint8_t*>(line1Plain.data()), line1Plain.size(),
					1, 1, true, salt.data(), wire1
				)), static_cast<int>(YEncDecryptor::Status::Ok));

				std::vector<uint8_t> wirePart;
				if (multipart)
				{
					std::string partPlain = "=ypart begin=1 end=16";
					BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
						reinterpret_cast<const uint8_t*>(partPlain.data()), partPlain.size(),
						1, 2, false, salt.data(), wirePart
					)), static_cast<int>(YEncDecryptor::Status::Ok));
				}

				// =yencryption placed on the vector's (wrong) line index.
				std::string headerPlain = "=yencryption cipher=XChaCha20-Poly1305 "
					"salt=1a2b3c4d5e6f7890abcdef1234567890 index=00000001 "
					"tag=0cd77ce245a654463f90b945b1d22d5b";
				std::vector<uint8_t> wireHeader;
				BOOST_REQUIRE_EQUAL(static_cast<int>(decryptor.EncryptControlLine(
					reinterpret_cast<const uint8_t*>(headerPlain.data()), headerPlain.size(),
					1, lineIndex, false, salt.data(), wireHeader
				)), static_cast<int>(YEncDecryptor::Status::Ok));

				std::string block;
				block.append(reinterpret_cast<const char*>(wire1.data()), wire1.size());
				block.append("\r\n");
				if (multipart)
				{
					block.append(reinterpret_cast<const char*>(wirePart.data()), wirePart.size());
					block.append("\r\n");
				}
				// Pad with filler so the header physically lands on lineIndex.
				while (blockerLineCount(block) < lineIndex - 1)
				{
					block.append("data\r\n");
				}
				block.append(reinterpret_cast<const char*>(wireHeader.data()), wireHeader.size());
				block.append("\r\n");
				block.append("data\r\n");
				block.append("=yend size=16\r\n");

				std::string clean;
				std::vector<uint8_t> outSalt;
				const bool ok = decryptor.RestoreControlLines(block.data(), block.size(), clean, outSalt) == YEncDecryptor::Status::Ok;
				BOOST_CHECK(!ok);
				BOOST_CHECK(clean.empty());
				CheckZeroOutput(vector, std::vector<uint8_t>(clean.begin(), clean.end()));
			}
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
