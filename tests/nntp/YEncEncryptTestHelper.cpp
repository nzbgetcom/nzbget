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
#include "YEncDecryptor.h"
#include "Ff1Primitives.h"

#ifdef NZBGET_TESTING_HOOKS

#include <sodium.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <vector>

using Ff1::ByteToNumeral;
using Ff1::NumeralToByte;
using Ff1::NumRadix;
using Ff1::StrRadix;
using Ff1::AesEcbEncrypt;
using Ff1::CbcMac;

namespace
{

struct EvpCipherCtxDeleter
{
	void operator()(EVP_CIPHER_CTX* ctx) const
	{
		if (ctx)
		{
			EVP_CIPHER_CTX_free(ctx);
		}
	}
};
using ScopedEvpCipherCtx = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

// FF1 encrypt (NIST SP 800-38G section 6, radix 253): the forward Feistel using the
// shared round-PRF scaffolding from Ff1Primitives.h. Test-only (see file head).
static std::vector<uint8_t> Ff1EncryptNumerals(
	const uint8_t key[32],
	const uint8_t* tweak, size_t tweakLen,
	const std::vector<uint8_t>& numerals,
	int radix = 253)
{
	ScopedEvpCipherCtx aesCtx(EVP_CIPHER_CTX_new());
	if (!aesCtx)
	{
		throw std::bad_alloc();
	}
	EVP_EncryptInit_ex(aesCtx.get(), EVP_aes_256_ecb(), nullptr, key, nullptr);
	EVP_CIPHER_CTX_set_padding(aesCtx.get(), 0);

	int n = static_cast<int>(numerals.size());
	int t = static_cast<int>(tweakLen);
	int u = n / 2;
	int v = n - u;

	std::vector<uint8_t> A(numerals.begin(), numerals.begin() + u);
	std::vector<uint8_t> B(numerals.begin() + u, numerals.end());

	for (int i = 0; i < 10; ++i)
	{
		std::vector<uint8_t> S = Ff1::RoundPrf(aesCtx.get(), radix, n, t, u, v,
			tweak, B, i);
		int d = static_cast<int>(S.size());
		BIGNUM* bnY = BN_bin2bn(S.data(), d, nullptr);
		BIGNUM* bnNumA = BN_new();
		BIGNUM* bnMod = BN_new();
		BIGNUM* bnC = BN_new();
		BN_CTX* ctx = BN_CTX_new();
		int m = (i % 2 == 0) ? u : v;
		Ff1::NumRadix(A, radix, bnNumA);
		BIGNUM* bnRadix = BN_new();
		BN_set_word(bnRadix, radix);
		BN_set_word(bnMod, m);
		BN_exp(bnMod, bnRadix, bnMod, ctx);
		BN_add(bnC, bnNumA, bnY);
		BN_nnmod(bnC, bnC, bnMod, ctx);
		std::vector<uint8_t> C = Ff1::StrRadix(bnC, radix, m);
		BN_free(bnY);
		BN_free(bnNumA);
		BN_free(bnMod);
		BN_free(bnC);
		BN_free(bnRadix);
		BN_CTX_free(ctx);
		A = B;
		B = C;
	}

	std::vector<uint8_t> result = A;
	result.insert(result.end(), B.begin(), B.end());
	return result;
}

} // anonymous namespace

YEncDecryptor::Status YEncDecryptor::EncryptControlLine(
	const uint8_t* plainData,
	size_t plainLen,
	uint32_t segmentIndex,
	uint32_t lineIndex,
	bool isLine1,
	const uint8_t salt[16],
	std::vector<uint8_t>& outWireData)
{
	outWireData.clear();
	if (plainLen < 2)
	{
		return Status::Error;
	}
	if (!EnsureMasterKey(salt))
	{
		return Status::Error;
	}

	uint8_t encKey[32];
	uint8_t tweak[8];
	if (!DeriveControlKeyAndTweak(segmentIndex, lineIndex, encKey, tweak))
	{
		return Status::Error;
	}

	std::vector<uint8_t> plainNumerals;
	plainNumerals.reserve(plainLen);
	try
	{
		for (size_t i = 0; i < plainLen; ++i)
		{
			plainNumerals.push_back(ByteToNumeral(plainData[i]));
		}
		const auto cipherNumerals = Ff1EncryptNumerals(encKey, tweak, 8, plainNumerals, 253);
		if (cipherNumerals.size() != plainNumerals.size())
		{
			outWireData.clear();
			sodium_memzero(encKey, sizeof(encKey));
			sodium_memzero(tweak, sizeof(tweak));
			return Status::Error;
		}
		if (isLine1)
		{
			outWireData.insert(outWireData.end(), salt, salt + 16);
			outWireData.push_back(static_cast<uint8_t>((segmentIndex >> 24) & 0xff));
			outWireData.push_back(static_cast<uint8_t>((segmentIndex >> 16) & 0xff));
			outWireData.push_back(static_cast<uint8_t>((segmentIndex >> 8) & 0xff));
			outWireData.push_back(static_cast<uint8_t>(segmentIndex & 0xff));
		}
		for (uint8_t numeral : cipherNumerals)
		{
			outWireData.push_back(NumeralToByte(numeral));
		}
	}
	catch (...)
	{
		outWireData.clear();
		sodium_memzero(encKey, sizeof(encKey));
		sodium_memzero(tweak, sizeof(tweak));
		return Status::Error;
	}
	sodium_memzero(encKey, sizeof(encKey));
	sodium_memzero(tweak, sizeof(tweak));
	return Status::Ok;
}

#endif // NZBGET_TESTING_HOOKS
