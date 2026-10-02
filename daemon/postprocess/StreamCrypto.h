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


#ifndef STREAMCRYPTO_H
#define STREAMCRYPTO_H

#include <memory>
#include "RarReader.h"	// RarFile::Rar5Crypt

/* Shared RAR key-derivation functions extracted from RarVolume so that both
 * archive-header decryption (RarReader) and, in later M3 tasks, stream repair
 * derive byte-identical keys from the same password + salt. Task 1 provides the
 * KDF layer; Task 2 adds the rar5 password-check derivation plus the
 * random-access CBC codecs exposed through RarCryptoContext below. */
class StreamCrypto
{
public:
	// RAR3 (AES-128-CBC) key schedule. Reproduces WinRAR's rar3 KDF exactly:
	// the password is expanded to UTF-16LE, concatenated with the 8-byte
	// per-file salt, then hashed with SHA-1 over 0x40000 rounds (each round
	// also folds in a 3-byte little-endian counter). The final digest is
	// byte-swizzled per 4-byte group into the 16-byte key; the 16 IV bytes are
	// sampled one-per-0x4000-rounds from the running digest's last byte.
	// Returns false on an empty password or when TLS (OpenSSL) is unavailable.
	static bool DeriveRar3(const char* password, const uint8 salt[8],
		uint8 keyOut[16], uint8 ivOut[16]);

	// RAR5 (AES-256-CBC) key schedule: PBKDF2-HMAC-SHA256 over the 16-byte salt
	// with 2^kdfCount iterations. Rejects kdfCount > 24 (a WinRAR bound; also
	// caps the iteration count). Returns false when TLS is unavailable.
	static bool DeriveRar5(const char* password, uint8 kdfCount,
		const uint8 salt[16], uint8 keyOut[32]);

	// RAR5 password-check value. unrar (crypt.cpp pbkdf2 / crypt5.cpp
	// SetKey50) derives the AES key with PBKDF2 at 2^kdfCount iterations, then
	// continues the SAME running XOR accumulator for 16 more iterations (the
	// archive HashKey) and 16 again to produce the 32-byte PswCheckValue. Since
	// a single PBKDF2 output block is exactly the XOR of U_1..U_c, that value
	// equals PBKDF2 at (2^kdfCount + 32) iterations; the stored 8-byte check is
	// that value folded by XOR into 8 bytes (check[i%8] ^= value[i]). Verified
	// byte-exact against testfile5encdata's stored check (password "123").
	// Returns false on kdfCount > 24 or when TLS is unavailable.
	static bool DeriveRar5PswCheck(const char* password, uint8 kdfCount,
		const uint8 salt[16], uint8 checkOut[8]);
};

/* A derived-key binding (key + header IV + AES variant) that decrypts or
 * encrypts arbitrary 16-byte-aligned block ranges of a RAR data/header stream
 * in raw CBC with no padding - matching RarVolume's own block codec. The unit
 * is whole cipher blocks so a caller can decrypt/re-encrypt a hole without
 * touching the rest of the stream: CBC decryption of block i needs only the
 * preceding ciphertext block C_{i-1} as an XOR input (random access is
 * trivial), and encryption chains forward from that same predecessor. */
class RarCryptoContext
{
public:
	static constexpr int CryptoBlockSize = 16;

	// rar3: derive AES-128 key + IV from the volume's 8-byte file-header salt.
	// nullptr on empty password or no TLS. There is no rar3 password check, so
	// a wrong password yields a context that decrypts to garbage (caught only
	// by the repair probes downstream, never here).
	static std::unique_ptr<RarCryptoContext> MakeRar3(const char* password,
		const uint8 salt[8]);

	// rar5: derive AES-256 key from the file's crypt record; the record's IV is
	// the header/chain-start IV. nullptr when the password is empty, TLS is
	// unavailable, the record is a non-AES256 version, or a stored password
	// check is present and does NOT match (wrong password => fail closed).
	static std::unique_ptr<RarCryptoContext> MakeRar5(const char* password,
		const RarFile::Rar5Crypt& crypt);

	// Decrypt `blocks` consecutive cipher blocks. prevCipherBlock is C_{i-1}
	// for the first block, or nullptr to chain from the header IV (block 0 of
	// the stream). Returns false on a negative count or when TLS is
	// unavailable; blocks == 0 is a no-op success. The interface is blocks-only
	// so a byte-misaligned length cannot be expressed.
	bool DecryptRange(const uint8* prevCipherBlock, const uint8* cipher,
		uint8* plain, int64 blocks) const;

	// Encrypt `blocks` consecutive plaintext blocks, chaining from
	// prevCipherBlock (nullptr = header IV). Same return contract as
	// DecryptRange.
	bool EncryptRange(const uint8* prevCipherBlock, const uint8* plain,
		uint8* cipher, int64 blocks) const;

private:
	RarCryptoContext() = default;
	bool Crypt(bool encrypt, const uint8* prevCipherBlock, const uint8* in,
		uint8* out, int64 blocks) const;

	int m_keyLength = 0;	// 16 (rar3/AES-128) or 32 (rar5/AES-256)
	uint8 m_key[32] = {};
	uint8 m_iv[16] = {};	// header IV: chain start for block 0
};

#endif
