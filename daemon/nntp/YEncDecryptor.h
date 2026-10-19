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

#ifndef YENC_DECRYPTOR_H
#define YENC_DECRYPTOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>

class YEncKeyCache;

class YEncDecryptor
{
public:
	enum class Status
	{
		Ok,
		AuthFailed,
		Error
	};

	// Hostile-control-line cap (Control Std v1.2 §9, Article-Bounded Memory):
	// Any control line larger than this is rejected before FF1 decryption,
	// which is quadratic in the line length (measured: 1 KiB ≈ 5 ms, 8 KiB ≈
	// 315 ms, 32 KiB ≈ 5 s on the reference ARM core). Legit control lines
	// (=ybegin, =ypart, =yencryption, =yend) are all short; 8 KiB leaves an
	// order of magnitude of headroom over the longest real-world filename
	// while bounding a hostile line to well under a second of CPU.
	// Enforced inside DecryptControlLine so every control-line decryption
	// (Line 1, =ypart, =yencryption, footer) is bounded.
	static constexpr size_t MAX_LINE1_PLAINTEXT_SIZE = 8 * 1024;

	explicit YEncDecryptor(const std::string& password = "", YEncKeyCache* keyCache = nullptr);
	~YEncDecryptor();

	const std::string& GetPassword() const { return m_password; }

	// Derives master key via Argon2id (RFC 9106: time=1, mem=64MB, parallelism=4, len=32),
	// caching in memory by 16-byte salt. When a job-owned YEncKeyCache is attached,
	// derived keys are shared (thread-safely) across all decryptors of the job.
	bool EnsureMasterKey(const uint8_t salt[16]);

	// Derives 24-byte body nonce using HMAC-SHA256 with 19-byte message layout:
	// "yenc-body nonce" (15 bytes) + uint32_be(segmentIndex) (4 bytes)
	bool DeriveBodyNonce(uint32_t segmentIndex, uint8_t outNonce[24]);

	// Derives control key (32 bytes) and tweak (8 bytes) for FF1:
	// key = HMAC-SHA256(masterKey, "yenc-control key")
	// tweak = HMAC-SHA256(masterKey, "yenc-control tweak" + uint32_be(segmentIndex) + uint32_be(lineIndex))[:8]
	bool DeriveControlKeyAndTweak(uint32_t segmentIndex, uint32_t lineIndex, uint8_t outKey[32], uint8_t outTweak[8]);

	// Authenticate and decrypt ciphertext using XChaCha20-Poly1305.
	// On tag mismatch, returns Status::AuthFailed and clears outPlaintext (Zero-Output Guarantee).
	Status AuthenticateAndDecrypt(
		const uint8_t* ciphertext,
		size_t cipherLen,
		const uint8_t salt[16],
		const uint8_t tag[16],
		uint32_t segmentIndex,
		const uint8_t* aad,
		size_t aadLen,
		std::vector<uint8_t>& outPlaintext
	);

	// Helper to build Section 2.1 Associated Data
	static std::vector<uint8_t> BuildAad(
		const std::string& name,
		uint32_t partNumber,
		uint32_t partBegin,
		uint64_t segmentBytes
	);

	// Decrypt control line using Radix-253 FF1.
	// If isLine1 == true, extracts 20-byte bootstrap ([16B salt][4B uint32_be(segmentIndex)])
	// from the wire data prefix and decrypts the remainder.
	Status DecryptControlLine(
		const uint8_t* wireData,
		size_t wireLen,
		uint32_t segmentIndex,
		uint32_t lineIndex,
		bool isLine1,
		std::vector<uint8_t>& outPlaintext,
		std::vector<uint8_t>* outSalt = nullptr,
		uint32_t* outSegmentIndex = nullptr
	);

	// Result of the most recent failed operation when it returned
	// Status::Error: a local (fatal) crypto failure such as sodium/Argon2id
	// failure or an oversized control line, as opposed to wire-byte
	// corruption that another provider could fix.
	bool WasLastErrorFatal() const { return m_lastErrorFatal; }

	struct YEncryptionHeader
	{
		std::vector<uint8_t> salt;
		std::vector<uint8_t> tag;
		uint32_t segmentIndex = 0;
	};

	static bool ParseYEncryption(
		const char* line,
		size_t lineLen,
		YEncryptionHeader& outHeader
	);

	// Restores the plaintext control lines of an encrypted wire block.
	// The authoritative segment index is extracted from the Line 1 bootstrap;
	// dual-bootstrap agreement with the =yencryption header is enforced here.
	// On Status::Error, WasLastErrorFatal() says whether the failure was a
	// local crypto failure (true) or wire corruption (false).
	Status RestoreControlLines(
		const char* wireBlock,
		size_t wireLen,
		std::string& outCleanBlock,
		std::vector<uint8_t>& outLine1Salt,
		YEncryptionHeader* outHeader = nullptr
	);

#ifdef NZBGET_TESTING_HOOKS
	// Test-only surface: uploads never run through the downloader, so the FF1
	// control-line encryptor exists only to build test fixtures from vendored
	// wire vectors. Never compiled into release builds (define
	// NZBGET_TESTING_HOOKS in the test target only).
	Status EncryptControlLine(
		const uint8_t* plainData,
		size_t plainLen,
		uint32_t segmentIndex,
		uint32_t lineIndex,
		bool isLine1,
		const uint8_t salt[16],
		std::vector<uint8_t>& outWireData
	);

	// Test-only surface: seeds the key cache directly so nonce/tweak vector
	// tests are deterministic without paying the 64 MiB Argon2id KDF per case.
	// Never compiled into release builds (define NZBGET_TESTING_HOOKS in the
	// test target only). The seeded key is still zeroized by ClearMasterKey.
	void SetMasterKeyForTesting(const uint8_t key[32], const uint8_t salt[16] = nullptr)
	{
		m_cachedMasterKey.assign(key, key + 32);
		if (salt)
		{
			m_cachedSalt.assign(salt, salt + 16);
		}
		else
		{
			m_cachedSalt.assign(16, 0);
		}
	}

	size_t GetCachedSaltCount() const { return m_cachedSalt.empty() ? 0 : 1; }
	const std::vector<uint8_t>& GetCachedMasterKey() const { return m_cachedMasterKey; }
#endif

private:
	std::string m_password;
	YEncKeyCache* m_keyCache = nullptr;
	bool m_sodiumInitFailed = false;
	bool m_lastErrorFatal = false;
	std::vector<uint8_t> m_cachedSalt;
	std::vector<uint8_t> m_cachedMasterKey; // 32 bytes, zeroized on destruction or reset

	void ClearMasterKey();
};

#endif // YENC_DECRYPTOR_H
