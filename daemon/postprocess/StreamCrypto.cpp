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

#include <cstring>
#include "StreamCrypto.h"
#include "NString.h"

#ifndef DISABLE_TLS
#include "OpenSSL.h"
#include <openssl/evp.h>
#endif

// This is a byte-exact extraction of RarVolume::DecryptRar3Prepare. The seed
// layout, the 0x40000-round SHA-1 schedule, the per-0x4000-round IV sampling
// point, and the 4-byte-group key swizzle must not drift: the encrypted-name
// (-hp) testdata only decrypts if this reproduces WinRAR's schedule exactly.
// Passwords are secrets, so unlike the original this function does NOT log the
// seed/key/IV (they are password-derived); the derived bytes are unchanged.
bool StreamCrypto::DeriveRar3(const char* password, const uint8 salt[8],
	uint8 keyOut[16], uint8 ivOut[16])
{
	WString wstr(password ? password : "");
	int len = wstr.Length();
	if (len == 0) return false;

	CharBuffer seed(len * 2 + 8);
	for (int i = 0; i < len; i++)
	{
		wchar_t ch = wstr[i];
		seed[i * 2] = ch & 0xFF;
		seed[i * 2 + 1] = (ch & 0xFF00) >> 8;
	}
	memcpy(seed + len * 2, salt, 8);

#ifndef DISABLE_TLS
	OpenSSL::EVPMdCtxPtr context{ EVP_MD_CTX_new(), &EVP_MD_CTX_free };
	if (!context || !EVP_DigestInit(context.get(), EVP_sha1()))
	{
		return false;
	}

	uint8 digest[20];
	const int rounds = 0x40000;

	for (int i = 0; i < rounds; i++)
	{
		EVP_DigestUpdate(context.get(), *seed, seed.Size());

		uint8 buf[3];
		buf[0] = (uint8)i;
		buf[1] = (uint8)(i >> 8);
		buf[2] = (uint8)(i >> 16);

		EVP_DigestUpdate(context.get(), buf, sizeof(buf));

		if (i % (rounds / 16) == 0)
		{
			OpenSSL::EVPMdCtxPtr ivContext{ EVP_MD_CTX_new(), &EVP_MD_CTX_free };
			if (ivContext)
			{
				EVP_MD_CTX_copy(ivContext.get(), context.get());
				EVP_DigestFinal(ivContext.get(), digest, nullptr);
			}
			ivOut[i / (rounds / 16)] = digest[sizeof(digest) - 1];
		}
	}

	EVP_DigestFinal(context.get(), digest, nullptr);

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			keyOut[i * 4 + j] = digest[i * 4 + 3 - j];
		}
	}

	return true;
#else
	(void)keyOut;
	(void)ivOut;
	return false;
#endif
}

// Byte-exact extraction of RarVolume::DecryptRar5Prepare.
bool StreamCrypto::DeriveRar5(const char* password, uint8 kdfCount,
	const uint8 salt[16], uint8 keyOut[32])
{
	if (kdfCount > 24) return false;

	int iterations = 1 << kdfCount;

#ifndef DISABLE_TLS
	const char* pwd = password ? password : "";
	if (!PKCS5_PBKDF2_HMAC(pwd, (int)strlen(pwd), salt, 16,
		iterations, EVP_sha256(), 32, keyOut)) return false;
	return true;
#else
	(void)salt;
	(void)keyOut;
	return false;
#endif
}

// RAR5 password check. See the header for the unrar-derived rationale: the
// stored 8-byte value is PBKDF2-HMAC-SHA256 at (2^kdfCount + 32) iterations
// (32 bytes) folded by XOR into 8 bytes. The trailing 4 bytes of the crypt
// record's CheckValue are an SHA-256 integrity csum of the stored 8 bytes (not
// password-derived) and are not reproduced here.
bool StreamCrypto::DeriveRar5PswCheck(const char* password, uint8 kdfCount,
	const uint8 salt[16], uint8 checkOut[8])
{
	if (kdfCount > 24) return false;

	// 2^kdfCount fits well within int for kdfCount <= 24, and +32 cannot overflow
	int iterations = (1 << kdfCount) + 32;

#ifndef DISABLE_TLS
	const char* pwd = password ? password : "";
	uint8 value[32];
	if (!PKCS5_PBKDF2_HMAC(pwd, (int)strlen(pwd), salt, 16,
		iterations, EVP_sha256(), 32, value)) return false;

	uint8 check[8] = {};
	for (int i = 0; i < 32; i++) check[i % 8] ^= value[i];
	memcpy(checkOut, check, 8);
	return true;
#else
	(void)salt;
	(void)checkOut;
	return false;
#endif
}

std::unique_ptr<RarCryptoContext> RarCryptoContext::MakeRar3(const char* password,
	const uint8 salt[8])
{
	if (!password || !*password) return nullptr;

	std::unique_ptr<RarCryptoContext> ctx(new RarCryptoContext());
	// DeriveRar3 rejects empty passwords / absent TLS and produces both the
	// AES-128 key and the CBC chain-start IV
	if (!StreamCrypto::DeriveRar3(password, salt, ctx->m_key, ctx->m_iv))
	{
		return nullptr;
	}
	ctx->m_keyLength = 16;
	return ctx;
}

std::unique_ptr<RarCryptoContext> RarCryptoContext::MakeRar5(const char* password,
	const RarFile::Rar5Crypt& crypt)
{
	if (!password || !*password) return nullptr;
	if (crypt.Version != 0) return nullptr;	// only AES-256 (crypt version 0)

	std::unique_ptr<RarCryptoContext> ctx(new RarCryptoContext());
	if (!StreamCrypto::DeriveRar5(password, crypt.KdfCount, crypt.Salt, ctx->m_key))
	{
		return nullptr;
	}

	// A stored password check gates the password: derive it and reject a
	// mismatch. Fail closed if the check cannot be derived (e.g. bad kdfCount).
	if (crypt.HasCheck)
	{
		uint8 check[8];
		if (!StreamCrypto::DeriveRar5PswCheck(password, crypt.KdfCount, crypt.Salt, check))
		{
			return nullptr;
		}
		if (memcmp(check, crypt.CheckValue, 8) != 0)
		{
			return nullptr;	// wrong password
		}
	}

	ctx->m_keyLength = 32;
	memcpy(ctx->m_iv, crypt.Iv, 16);	// crypt-record IV is the chain start
	return ctx;
}

// Raw CBC with padding disabled, matching RarVolume's own block codec. The
// context is created per call and seeded with prevCipherBlock (or the header IV
// for block 0); OpenSSL then chains internally across the whole block-aligned
// range, so decrypting or encrypting an arbitrary sub-range is a single pass
// once the caller supplies the preceding ciphertext block.
bool RarCryptoContext::Crypt(bool encrypt, const uint8* prevCipherBlock,
	const uint8* in, uint8* out, int64 blocks) const
{
	if (blocks < 0) return false;
	if (blocks == 0) return true;	// nothing to do
	if (!in || !out) return false;

#ifndef DISABLE_TLS
	const uint8* iv = prevCipherBlock ? prevCipherBlock : m_iv;

	OpenSSL::EVPCipherCtxPtr ctx{ EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free };
	if (!ctx) return false;

	const EVP_CIPHER* cipher = m_keyLength == 16 ? EVP_aes_128_cbc() : EVP_aes_256_cbc();
	int ok = encrypt
		? EVP_EncryptInit(ctx.get(), cipher, m_key, iv)
		: EVP_DecryptInit(ctx.get(), cipher, m_key, iv);
	if (!ok) return false;
	EVP_CIPHER_CTX_set_padding(ctx.get(), 0);

	// process in batches so the EVP int length argument never overflows on a
	// large range; CBC chaining is preserved across Update calls on one context
	const int64 batchBlocks = 1 << 16;	// 64 Ki blocks = 1 MiB per pass
	int64 done = 0;
	while (done < blocks)
	{
		int64 batch = blocks - done < batchBlocks ? blocks - done : batchBlocks;
		int len = (int)(batch * CryptoBlockSize);
		int outlen = 0;
		int step = encrypt
			? EVP_EncryptUpdate(ctx.get(), out + done * CryptoBlockSize, &outlen,
				in + done * CryptoBlockSize, len)
			: EVP_DecryptUpdate(ctx.get(), out + done * CryptoBlockSize, &outlen,
				in + done * CryptoBlockSize, len);
		if (!step || outlen != len) return false;	// padding off => outlen == len
		done += batch;
	}
	return true;
#else
	(void)encrypt;
	(void)prevCipherBlock;
	return false;
#endif
}

bool RarCryptoContext::DecryptRange(const uint8* prevCipherBlock, const uint8* cipher,
	uint8* plain, int64 blocks) const
{
	return Crypt(false, prevCipherBlock, cipher, plain, blocks);
}

bool RarCryptoContext::EncryptRange(const uint8* prevCipherBlock, const uint8* plain,
	uint8* cipher, int64 blocks) const
{
	return Crypt(true, prevCipherBlock, plain, cipher, blocks);
}
