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
