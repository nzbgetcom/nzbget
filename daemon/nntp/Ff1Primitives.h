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

#ifndef FF1_PRIMITIVES_H
#define FF1_PRIMITIVES_H

// FF1 building blocks shared between the production FF1 decrypt path
// (YEncDecryptor.cpp) and the test-only FF1 encrypt helper
// (YEncEncryptTestHelper.cpp). Moved here so the radix-253 mapping, BIGNUM
// radix conversion, and CBC-MAC primitives exist exactly once.
//
// The FF1 round function itself stays split: only the decrypt direction ships
// in production; the encrypt direction is compiled into the test target only
// (NZBGET_TESTING_HOOKS) because downloads never encrypt.

#include "nzbget.h"

#include <openssl/evp.h>
#include <openssl/bn.h>

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <memory>
#include <vector>
#include <stdexcept>
#include <new>

namespace Ff1
{

struct BnCtxDeleter
{
	void operator()(BN_CTX* ctx) const
	{
		if (ctx)
		{
			BN_CTX_free(ctx);
		}
	}
};
using ScopedBnCtx = std::unique_ptr<BN_CTX, BnCtxDeleter>;

struct BignumDeleter
{
	void operator()(BIGNUM* bn) const
	{
		if (bn)
		{
			BN_free(bn);
		}
	}
};
using ScopedBignum = std::unique_ptr<BIGNUM, BignumDeleter>;

// Radix-253 byte <-> numeral mapping (Control Std v1.2 §4: forbidden 0x00,
// 0x0A, 0x0D and the '=' escape shrink the alphabet to 253 symbols).
inline uint8_t ByteToNumeral(uint8_t b)
{
	if (b >= 0x01 && b <= 0x09) return b - 1;
	if (b == 0x0B) return 9;
	if (b == 0x0C) return 10;
	if (b >= 0x0E) return b - 3;
	throw std::runtime_error("Forbidden byte outside 253-byte Alphabet");
}

inline uint8_t NumeralToByte(uint8_t num)
{
	if (num <= 8) return num + 1;
	if (num == 9) return 0x0B;
	if (num == 10) return 0x0C;
	if (num <= 252) return num + 3;
	throw std::runtime_error("Numeral out of range [0, 252]");
}

inline void NumRadix(const std::vector<uint8_t>& numerals, int radix, BIGNUM* outBn)
{
	BN_zero(outBn);
	for (uint8_t n : numerals)
	{
		BN_mul_word(outBn, radix);
		BN_add_word(outBn, n);
	}
}

inline std::vector<uint8_t> StrRadix(const BIGNUM* inBn, int radix, int m)
{
	std::vector<uint8_t> res(m, 0);
	ScopedBignum val(BN_dup(inBn));
	if (!val)
	{
		throw std::bad_alloc();
	}
	for (int i = 0; i < m; ++i)
	{
		BN_ULONG rem = BN_div_word(val.get(), radix);
		res[m - 1 - i] = static_cast<uint8_t>(rem);
	}
	return res;
}

inline void AesEcbEncrypt(EVP_CIPHER_CTX* ctx, const uint8_t in[16], uint8_t out[16])
{
	int outl = 0;
	EVP_EncryptUpdate(ctx, out, &outl, in, 16);
}

inline std::vector<uint8_t> CbcMac(EVP_CIPHER_CTX* ctx, const std::vector<uint8_t>& data)
{
	std::vector<uint8_t> block(16, 0);
	uint8_t tmp[16];
	for (size_t i = 0; i < data.size(); i += 16)
	{
		for (int k = 0; k < 16; ++k)
		{
			tmp[k] = block[k] ^ data[i + k];
		}
		AesEcbEncrypt(ctx, tmp, block.data());
	}
	return block;
}

// Shared FF1 round scaffolding: computes the per-round ciphertext block S for
// the given half, tweak and round index (Feistel PRF, NIST SP 800-38G §6).
// Returns S padded to the exact required byte width d (multiple of 16 plus 4).
inline std::vector<uint8_t> RoundPrf(EVP_CIPHER_CTX* aesCtx,
	int radix, int n, int t, int u, int v, const uint8_t* tweak,
	const std::vector<uint8_t>& half, int roundIdx)
{
	int b = static_cast<int>(std::ceil(std::ceil(v * std::log2(static_cast<double>(radix))) / 8.0));
	int d = 4 * static_cast<int>(std::ceil(b / 4.0)) + 4;

	std::vector<uint8_t> p(16, 0);
	p[0] = 1; p[1] = 2; p[2] = 1;
	p[3] = (radix >> 16) & 0xFF;
	p[4] = (radix >> 8) & 0xFF;
	p[5] = radix & 0xFF;
	p[6] = 10;
	p[7] = u % 256;
	p[8] = (n >> 24) & 0xFF; p[9] = (n >> 16) & 0xFF; p[10] = (n >> 8) & 0xFF; p[11] = n & 0xFF;
	p[12] = (t >> 24) & 0xFF; p[13] = (t >> 16) & 0xFF; p[14] = (t >> 8) & 0xFF; p[15] = t & 0xFF;

	int padLen = ((-t - b - 1) % 16 + 16) % 16;
	std::vector<uint8_t> qPrefix;
	qPrefix.insert(qPrefix.end(), tweak, tweak + static_cast<size_t>(t));
	qPrefix.insert(qPrefix.end(), padLen, 0);

	std::vector<uint8_t> numHalfBytes(b, 0);
	ScopedBignum numHalf(BN_new());
	if (!numHalf)
	{
		throw std::bad_alloc();
	}
	NumRadix(half, radix, numHalf.get());
	BN_bn2binpad(numHalf.get(), numHalfBytes.data(), b);

	std::vector<uint8_t> q = qPrefix;
	q.push_back(static_cast<uint8_t>(roundIdx));
	q.insert(q.end(), numHalfBytes.begin(), numHalfBytes.end());

	std::vector<uint8_t> pq = p;
	pq.insert(pq.end(), q.begin(), q.end());

	std::vector<uint8_t> mac = CbcMac(aesCtx, pq);
	std::vector<uint8_t> prf = mac;
	uint32_t j = 1;
	while (prf.size() < static_cast<size_t>(d))
	{
		uint8_t jBytes[16] = {0};
		jBytes[12] = (j >> 24) & 0xFF;
		jBytes[13] = (j >> 16) & 0xFF;
		jBytes[14] = (j >> 8) & 0xFF;
		jBytes[15] = j & 0xFF;

		uint8_t blk[16];
		for (int k = 0; k < 16; ++k) blk[k] = mac[k] ^ jBytes[k];
		uint8_t encBlk[16];
		AesEcbEncrypt(aesCtx, blk, encBlk);
		prf.insert(prf.end(), encBlk, encBlk + 16);
		j++;
	}
	// NIST SP 800-38G §6: only the first d bytes of prf enter the modular
	// arithmetic; prf.size() can exceed d because blocks are appended in
	// 16-byte units.
	prf.resize(d);
	return prf;
}

} // namespace Ff1

#endif // FF1_PRIMITIVES_H
