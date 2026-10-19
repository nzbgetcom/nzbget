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
#include "YEncKeyCache.h"
#include "Ff1Primitives.h"
#include "Log.h"

#include <sodium.h>
#include <argon2.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#include <cstring>
#include <memory>

using Ff1::ByteToNumeral;
using Ff1::NumeralToByte;

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

static bool IsStrictLowerHex(const char* s, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		char c = s[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
		{
			return false;
		}
	}
	return true;
}

// FF1 decrypt (NIST SP 800-38G §6, radix 253): runs the 10-round Feistel
// backwards using the shared round-PRF scaffolding from Ff1Primitives.h.
static std::vector<uint8_t> Ff1DecryptNumerals(
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

	std::vector<uint8_t> a(numerals.begin(), numerals.begin() + u);
	std::vector<uint8_t> b(numerals.begin() + u, numerals.end());

	for (int roundIdx = 0; roundIdx < 10; ++roundIdx)
	{
		int i = 9 - roundIdx;
		std::vector<uint8_t> s = Ff1::RoundPrf(aesCtx.get(), radix, n, t, u, v,
			tweak, a, i);
		int d = static_cast<int>(s.size());
		Ff1::ScopedBignum bnY(BN_bin2bn(s.data(), d, nullptr));
		Ff1::ScopedBignum bnNumB(BN_new());
		Ff1::ScopedBignum bnMod(BN_new());
		Ff1::ScopedBignum bnC(BN_new());
		Ff1::ScopedBignum bnRadix(BN_new());
		Ff1::ScopedBnCtx ctx(BN_CTX_new());
		if (!bnY || !bnNumB || !bnMod || !bnC || !bnRadix || !ctx)
		{
			throw std::bad_alloc();
		}
		int m = (i % 2 == 0) ? u : v;
		Ff1::NumRadix(b, radix, bnNumB.get());
		BN_set_word(bnRadix.get(), radix);
		BN_set_word(bnMod.get(), m);
		BN_exp(bnMod.get(), bnRadix.get(), bnMod.get(), ctx.get());
		BN_sub(bnC.get(), bnNumB.get(), bnY.get());
		BN_nnmod(bnC.get(), bnC.get(), bnMod.get(), ctx.get());
		std::vector<uint8_t> c = Ff1::StrRadix(bnC.get(), radix, m);
		b = a;
		a = c;
	}

	std::vector<uint8_t> result = a;
	result.insert(result.end(), b.begin(), b.end());
	return result;
}

static bool HexToBytesStrict(const char* hex, size_t hexLen, uint8_t* outBytes, size_t outLen)
{
	if (hexLen != outLen * 2)
	{
		return false;
	}
	if (!IsStrictLowerHex(hex, hexLen))
	{
		return false;
	}
	for (size_t i = 0; i < outLen; ++i)
	{
		auto hexVal = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			return -1;
		};
		int hi = hexVal(hex[i * 2]);
		int lo = hexVal(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
		{
			return false;
		}
		outBytes[i] = static_cast<uint8_t>((hi << 4) | lo);
	}
	return true;
}

} // anonymous namespace

YEncDecryptor::YEncDecryptor(const std::string& password, YEncKeyCache* keyCache)
	: m_password(password)
	, m_keyCache(keyCache)
{
	// A libsodium initialization failure is a hard crypto error, not a
	// condition to silently continue from — every subsequent AEAD/FF1 call
	// would misbehave. Record it as a typed failure consumed by the Status
	// surface (Error) instead of being swallowed.
	m_sodiumInitFailed = sodium_init() < 0;
	if (m_sodiumInitFailed)
	{
		error("Failed to initialize libsodium (sodium_init() < 0); decryption is unavailable");
	}
}

YEncDecryptor::~YEncDecryptor()
{
	ClearMasterKey();
}

void YEncDecryptor::ClearMasterKey()
{
	if (!m_cachedMasterKey.empty())
	{
		sodium_memzero(m_cachedMasterKey.data(), m_cachedMasterKey.size());
		m_cachedMasterKey.clear();
	}
	m_cachedSalt.clear();
}

bool YEncDecryptor::EnsureMasterKey(const uint8_t salt[16])
{
	if (!m_cachedSalt.empty() && m_cachedSalt.size() == 16 && memcmp(m_cachedSalt.data(), salt, 16) == 0)
	{
		return true;
	}

	if (m_password.empty())
	{
		return false;
	}

	ClearMasterKey();

	uint8_t key[32];
	auto derive = [&](uint8_t* output)
	{
		return argon2id_hash_raw(1, 65536, 4, m_password.data(), m_password.size(), salt, 16, output, 32) == ARGON2_OK;
	};
	if (!(m_keyCache ? m_keyCache->GetOrDeriveKey(salt, m_password, key, derive) : derive(key)))
	{
		sodium_memzero(key, sizeof(key));
		return false;
	}

	m_cachedSalt.assign(salt, salt + 16);
	m_cachedMasterKey.assign(key, key + 32);
	sodium_memzero(key, sizeof(key));
	return true;
}

bool YEncDecryptor::DeriveBodyNonce(uint32_t segmentIndex, uint8_t outNonce[24])
{
	if (m_cachedMasterKey.size() != 32)
	{
		return false;
	}

	uint8_t msg[19];
	memcpy(msg, "yenc-body nonce", 15);
	msg[15] = static_cast<uint8_t>((segmentIndex >> 24) & 0xFF);
	msg[16] = static_cast<uint8_t>((segmentIndex >> 16) & 0xFF);
	msg[17] = static_cast<uint8_t>((segmentIndex >> 8) & 0xFF);
	msg[18] = static_cast<uint8_t>(segmentIndex & 0xFF);

	// HMAC() returns a pointer to the digest or nullptr on failure (e.g.
	// allocation/engine error). Copying an uninitialized stack buffer would
	// leak garbage into the AEAD nonce, so fail closed via the existing
	// Status::Error mapping instead.
	unsigned int mdLen = 0;
	uint8_t md[EVP_MAX_MD_SIZE];
	sodium_memzero(md, sizeof(md));
	const unsigned char* hmacResult = HMAC(EVP_sha256(), m_cachedMasterKey.data(), 32, msg, 19, md, &mdLen);
	if (hmacResult == nullptr || mdLen < 24)
	{
		sodium_memzero(md, sizeof(md));
		return false;
	}
	memcpy(outNonce, md, 24);
	sodium_memzero(md, sizeof(md));
	return true;
}

bool YEncDecryptor::DeriveControlKeyAndTweak(uint32_t segmentIndex, uint32_t lineIndex, uint8_t outKey[32], uint8_t outTweak[8])
{
	if (m_cachedMasterKey.size() != 32)
	{
		return false;
	}

	unsigned int mdLen = 0;
	uint8_t md[EVP_MAX_MD_SIZE];
	sodium_memzero(md, sizeof(md));
	const unsigned char* hmacKey = HMAC(EVP_sha256(), m_cachedMasterKey.data(), 32, reinterpret_cast<const unsigned char*>("yenc-control key"), 16, md, &mdLen);
	if (hmacKey == nullptr || mdLen < 32)
	{
		sodium_memzero(md, sizeof(md));
		return false;
	}
	memcpy(outKey, md, 32);
	sodium_memzero(md, sizeof(md));

	uint8_t tweakMsg[26];
	memcpy(tweakMsg, "yenc-control tweak", 18);
	tweakMsg[18] = static_cast<uint8_t>((segmentIndex >> 24) & 0xFF);
	tweakMsg[19] = static_cast<uint8_t>((segmentIndex >> 16) & 0xFF);
	tweakMsg[20] = static_cast<uint8_t>((segmentIndex >> 8) & 0xFF);
	tweakMsg[21] = static_cast<uint8_t>(segmentIndex & 0xFF);
	tweakMsg[22] = static_cast<uint8_t>((lineIndex >> 24) & 0xFF);
	tweakMsg[23] = static_cast<uint8_t>((lineIndex >> 16) & 0xFF);
	tweakMsg[24] = static_cast<uint8_t>((lineIndex >> 8) & 0xFF);
	tweakMsg[25] = static_cast<uint8_t>(lineIndex & 0xFF);

	const unsigned char* hmacTweak = HMAC(EVP_sha256(), m_cachedMasterKey.data(), 32, tweakMsg, 26, md, &mdLen);
	if (hmacTweak == nullptr || mdLen < 8)
	{
		sodium_memzero(md, sizeof(md));
		return false;
	}
	memcpy(outTweak, md, 8);
	sodium_memzero(md, sizeof(md));
	return true;
}

YEncDecryptor::Status YEncDecryptor::AuthenticateAndDecrypt(
	const uint8_t* ciphertext,
	size_t cipherLen,
	const uint8_t salt[16],
	const uint8_t tag[16],
	uint32_t segmentIndex,
	const uint8_t* aad,
	size_t aadLen,
	std::vector<uint8_t>& outPlaintext)
{
	outPlaintext.clear();
	m_lastErrorFatal = false;

	// A sodium_init() failure recorded at construction surfaces as a
	// typed Error on the first crypto call instead of undefined behavior.
	if (m_sodiumInitFailed)
	{
		m_lastErrorFatal = true;
		return Status::Error;
	}

	if (!EnsureMasterKey(salt))
	{
		m_lastErrorFatal = true;
		return Status::Error;
	}

	uint8_t nonce[24];
	if (!DeriveBodyNonce(segmentIndex, nonce))
	{
		m_lastErrorFatal = true;
		return Status::Error;
	}

	// Detached AEAD (libsodium): ciphertext and tag passed separately, so no
	// article-sized ct||tag copy is needed (Body Std v1.2 §4: tag is a
	// 16-byte independent field of =yencryption).
	outPlaintext.resize(cipherLen);
	int res = crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
		cipherLen > 0 ? outPlaintext.data() : nullptr, nullptr,
		ciphertext, cipherLen,
		tag,
		aad, aadLen,
		nonce, m_cachedMasterKey.data());

	sodium_memzero(nonce, sizeof(nonce));

	if (res != 0)
	{
		sodium_memzero(outPlaintext.data(), outPlaintext.size());
		outPlaintext.clear();
		return Status::AuthFailed;
	}

	return Status::Ok;
}

std::vector<uint8_t> YEncDecryptor::BuildAad(
	const std::string& name,
	uint32_t partNumber,
	uint32_t partBegin,
	uint64_t segmentBytes)
{
	// Exact AAD serialization (Body Standard Section 2.1):
	// "yenc-body aad" (13B) + uint32_be(partNumber) (4B) + uint32_be(partBegin) (4B)
	// + uint64_be(segmentBytes) (8B) + uint32_be(nameLength) (4B) + nameBytes
	if (name.empty())
	{
		return {};
	}
	std::vector<uint8_t> aad;
	aad.reserve(33 + name.size());
	const char prefix[] = "yenc-body aad";
	aad.insert(aad.end(), reinterpret_cast<const uint8_t*>(prefix), reinterpret_cast<const uint8_t*>(prefix) + 13);

	aad.push_back(static_cast<uint8_t>((partNumber >> 24) & 0xFF));
	aad.push_back(static_cast<uint8_t>((partNumber >> 16) & 0xFF));
	aad.push_back(static_cast<uint8_t>((partNumber >> 8) & 0xFF));
	aad.push_back(static_cast<uint8_t>(partNumber & 0xFF));

	aad.push_back(static_cast<uint8_t>((partBegin >> 24) & 0xFF));
	aad.push_back(static_cast<uint8_t>((partBegin >> 16) & 0xFF));
	aad.push_back(static_cast<uint8_t>((partBegin >> 8) & 0xFF));
	aad.push_back(static_cast<uint8_t>(partBegin & 0xFF));

	for (int shift = 56; shift >= 0; shift -= 8)
	{
		aad.push_back(static_cast<uint8_t>((segmentBytes >> shift) & 0xFF));
	}

	uint32_t nameLen = static_cast<uint32_t>(name.size());
	aad.push_back(static_cast<uint8_t>((nameLen >> 24) & 0xFF));
	aad.push_back(static_cast<uint8_t>((nameLen >> 16) & 0xFF));
	aad.push_back(static_cast<uint8_t>((nameLen >> 8) & 0xFF));
	aad.push_back(static_cast<uint8_t>(nameLen & 0xFF));

	aad.insert(aad.end(), reinterpret_cast<const uint8_t*>(name.data()), reinterpret_cast<const uint8_t*>(name.data()) + name.size());
	return aad;
}

YEncDecryptor::Status YEncDecryptor::DecryptControlLine(
	const uint8_t* wireData,
	size_t wireLen,
	uint32_t segmentIndex,
	uint32_t lineIndex,
	bool isLine1,
	std::vector<uint8_t>& outPlaintext,
	std::vector<uint8_t>* outSalt,
	uint32_t* outSegmentIndex)
{
	outPlaintext.clear();
	m_lastErrorFatal = false;

	if (m_sodiumInitFailed)
	{
		m_lastErrorFatal = true;
		return Status::Error;
	}

	// Hostile-Line cap (Control Std v1.2 §9, Article-Bounded Memory):
	// FF1 decryption is quadratic in the line length (the round PRF
	// converts the whole half to a BIGNUM each of the 10 rounds), so a
	// multi-megabyte hostile line would tie up a download thread for
	// hours. Reject before key derivation/FF1; every legitimate control
	// line is short (Line 1 is "=ybegin line=<n> size=<n> name=<filename>",
	// even pathological names stay far below the cap). All encrypted
	// control lines go through here, so the cap bounds every FF1
	// decryption, not just Line 1.
	if (wireLen > MAX_LINE1_PLAINTEXT_SIZE)
	{
		m_lastErrorFatal = true;
		return Status::Error;
	}

	const uint8_t* ctBytes = wireData;
	size_t ctLen = wireLen;
	uint32_t effectiveSegmentIndex = segmentIndex;
	std::vector<uint8_t> extractedSalt;

	if (isLine1)
	{
		// 16B salt + 4B uint32_be(segmentIndex) + at least 2B FF1 ciphertext = 22 bytes minimum
		if (wireLen < 22)
		{
			return Status::Error;
		}
		const uint8_t* salt = wireData;
		for (size_t i = 0; i < 16; ++i)
		{
			if (salt[i] == 0x00 || salt[i] == 0x0A || salt[i] == 0x0D)
			{
				return Status::Error;
			}
		}

		uint32_t extractedIndex =
			(static_cast<uint32_t>(wireData[16]) << 24) |
			(static_cast<uint32_t>(wireData[17]) << 16) |
			(static_cast<uint32_t>(wireData[18]) << 8) |
			static_cast<uint32_t>(wireData[19]);

		if (extractedIndex == 0)
		{
			return Status::Error;
		}

		// Index framing rule (Body Std v1.2 §8): any uint32_be(segmentIndex) byte
		// 0x0A/0x0D would split Line 1 on the wire. Reject the bootstrap instead of decrypting.
		if (wireData[16] == 0x0A || wireData[16] == 0x0D ||
			wireData[17] == 0x0A || wireData[17] == 0x0D ||
			wireData[18] == 0x0A || wireData[18] == 0x0D ||
			wireData[19] == 0x0A || wireData[19] == 0x0D)
		{
			error("Bootstrap extraction failed: FORBIDDEN_SEGMENT_INDEX_BYTE");
			return Status::Error;
		}

		if (!EnsureMasterKey(salt))
		{
			m_lastErrorFatal = true;
			return Status::Error;
		}
		// NG12-4: publish bootstrap outputs only on Status::Ok (below).
		// Keep them in locals so a later Derive/FF1/numeral failure cannot
		// leak a salt/index the caller would treat as authenticated.
		extractedSalt.assign(salt, salt + 16);

		effectiveSegmentIndex = extractedIndex;
		ctBytes = wireData + 20;
		ctLen = wireLen - 20;
	}
	else
	{
		if (wireLen < 2)
		{
			return Status::Error;
		}
		if (m_cachedMasterKey.empty())
		{
			return Status::Error;
		}
	}

	if (ctLen < 2)
	{
		return Status::Error;
	}

	uint8_t encKey[32];
	uint8_t tweak[8];
	if (!DeriveControlKeyAndTweak(effectiveSegmentIndex, lineIndex, encKey, tweak))
	{
		// HMAC failure is a local crypto failure (or, for a non-Line-1 line,
		// a missing master key that an earlier local failure prevented).
		m_lastErrorFatal = true;
		return Status::Error;
	}

	std::vector<uint8_t> ctNumerals;
	ctNumerals.reserve(ctLen);
	for (size_t i = 0; i < ctLen; ++i)
	{
		try
		{
			ctNumerals.push_back(ByteToNumeral(ctBytes[i]));
		}
		catch (...)
		{
			sodium_memzero(encKey, sizeof(encKey));
			return Status::Error;
		}
	}

	std::vector<uint8_t> ptNumerals;
	try
	{
		ptNumerals = Ff1DecryptNumerals(encKey, tweak, 8, ctNumerals, 253);
	}
	catch (...)
	{
		// FF1 failures here are local (bad_alloc from EVP/BIGNUM/vector
		// allocation), not wire corruption: the ciphertext numerals were
		// already validated by ByteToNumeral above.
		sodium_memzero(encKey, sizeof(encKey));
		m_lastErrorFatal = true;
		return Status::Error;
	}
	sodium_memzero(encKey, sizeof(encKey));

	if (ptNumerals.size() != ctNumerals.size())
	{
		return Status::Error;
	}

	outPlaintext.reserve(ptNumerals.size());
	for (uint8_t n : ptNumerals)
	{
		try
		{
			outPlaintext.push_back(NumeralToByte(n));
		}
		catch (const std::bad_alloc&)
		{
			outPlaintext.clear();
			m_lastErrorFatal = true;
			return Status::Error;
		}
		catch (...)
		{
			outPlaintext.clear();
			return Status::Error;
		}
	}

	if (isLine1)
	{
		if (outSalt)
		{
			outSalt->assign(extractedSalt.begin(), extractedSalt.end());
		}
		if (outSegmentIndex)
		{
			*outSegmentIndex = effectiveSegmentIndex;
		}
	}

	return Status::Ok;
}

bool YEncDecryptor::ParseYEncryption(
	const char* line,
	size_t lineLen,
	YEncryptionHeader& outHeader)
{
	if (!line || lineLen < 13)
	{
		return false;
	}
	while (lineLen > 0 && (line[lineLen - 1] == '\r' || line[lineLen - 1] == '\n'))
	{
		--lineLen;
	}

	std::vector<std::string_view> tokens;
	size_t start = 0;
	for (size_t i = 0; i <= lineLen; ++i)
	{
		if (i == lineLen || line[i] == ' ')
		{
			if (i == start)
			{
				return false;
			}
			tokens.emplace_back(line + start, i - start);
			start = i + 1;
		}
	}
	if (tokens.size() != 5 || tokens[0] != "=yencryption" || tokens[1] != "cipher=XChaCha20-Poly1305")
	{
		return false;
	}
	if (tokens[2].rfind("salt=", 0) != 0 || tokens[3].rfind("index=", 0) != 0 || tokens[4].rfind("tag=", 0) != 0)
	{
		return false;
	}
	const std::string_view saltHex = tokens[2].substr(5);
	const std::string_view indexHex = tokens[3].substr(6);
	const std::string_view tagHex = tokens[4].substr(4);
	if (saltHex.size() != 32 || indexHex.size() != 8 || tagHex.size() != 32 ||
		!IsStrictLowerHex(saltHex.data(), saltHex.size()) ||
		!IsStrictLowerHex(indexHex.data(), indexHex.size()) ||
		!IsStrictLowerHex(tagHex.data(), tagHex.size()))
	{
		return false;
	}

	uint32_t parsedIndex = 0;
	for (char c : indexHex)
	{
		parsedIndex = (parsedIndex << 4) | (c >= 'a' ? (c - 'a' + 10) : (c - '0'));
	}
	if (parsedIndex == 0)
	{
		return false;
	}
	// Index framing rule (Body Std v1.2 §8): reject any index whose
	// uint32_be bytes contain 0x0A or 0x0D (PROVIDER_FAILOVER).
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		const uint8_t indexByte = static_cast<uint8_t>((parsedIndex >> shift) & 0xFF);
		if (indexByte == 0x0A || indexByte == 0x0D)
		{
			return false;
		}
	}

	outHeader.salt.resize(16);
	outHeader.tag.resize(16);
	outHeader.segmentIndex = parsedIndex;
	return HexToBytesStrict(saltHex.data(), saltHex.size(), outHeader.salt.data(), outHeader.salt.size()) &&
		HexToBytesStrict(tagHex.data(), tagHex.size(), outHeader.tag.data(), outHeader.tag.size());
}

YEncDecryptor::Status YEncDecryptor::RestoreControlLines(
	const char* wireBlock,
	size_t wireLen,
	std::string& outCleanBlock,
	std::vector<uint8_t>& outLine1Salt,
	YEncryptionHeader* outHeader)
{
	// Header-loop semantics (Control Std v1.2 §5 step 4): this decoder uses
	// the structural-assumption approach — it decrypts the known control-line
	// positions (Line 1 =ybegin, optional =ypart, =yencryption, footer =yend)
	// instead of probing each middle line. For canonical producers this is
	// conformant: only decryption success yielding non-=y content would
	// terminate the header loop, and any structural mismatch (including a
	// misplaced =yencryption, see the placement vectors) fails closed here as
	// PROVIDER_FAILOVER — no FF1 error is ever passed through as a data line.
	outCleanBlock.clear();
	outLine1Salt.clear();
	m_lastErrorFatal = false;
	if (!wireBlock || wireLen == 0)
	{
		return Status::Error;
	}

	// Only lines 1-3 and the footer need random access; the body between
	// them streams through verbatim, so no per-line index is materialised
	// (Body Std v1.2 §9 / NG12-1: a 16 MiB block of separators must not
	// allocate ~16M line descriptors). First/second/third are found by a
	// forward scan; the footer is the last non-empty physical line (Control
	// Std v1.2 §5 as amended by STD12-2: trailing empty lines are ignored
	// before footer counting), found by a backward scan. Logical line
	// counting still counts every physical line, so the footer's lineIndex
	// is exact and every body line is checked for a smuggled =yencryption.
	struct WireLine
	{
		const char* data = nullptr;
		size_t len = 0;
		std::string_view ending;
	};
	auto scanLine = [](const char* cursor, const char* endPos, WireLine& out)
	{
		const char* newline = static_cast<const char*>(memchr(cursor, '\n', endPos - cursor));
		const char* lineEnd = newline ? newline : endPos;
		const bool crlf = lineEnd > cursor && lineEnd[-1] == '\r';
		out.data = cursor;
		out.len = static_cast<size_t>((crlf ? lineEnd - 1 : lineEnd) - cursor);
		out.ending = newline ? (crlf ? std::string_view("\r\n", 2) : std::string_view("\n", 1)) : std::string_view();
		return newline ? newline + 1 : endPos;
	};
	auto previousLine = [](const char* cursor, const char* beginPos, WireLine& out)
	{
		// cursor is one past the candidate line's ending (end of block on
		// the first call, then the start of the line just examined).
		const char* contentEnd = cursor;
		std::string_view ending;
		if (contentEnd > beginPos && contentEnd[-1] == '\n')
		{
			--contentEnd;
			if (contentEnd > beginPos && contentEnd[-1] == '\r')
			{
				--contentEnd;
				ending = std::string_view("\r\n", 2);
			}
			else
			{
				ending = std::string_view("\n", 1);
			}
		}
		const char* lineStart = contentEnd;
		while (lineStart > beginPos && lineStart[-1] != '\n')
		{
			--lineStart;
		}
		out.data = lineStart;
		out.len = static_cast<size_t>(contentEnd - lineStart);
		out.ending = ending;
		return lineStart;
	};

	WireLine firstLine;
	const char* afterFirst = scanLine(wireBlock, wireBlock + wireLen, firstLine);
	WireLine secondProbe;
	scanLine(afterFirst, wireBlock + wireLen, secondProbe);
	const bool secondOversized = secondProbe.len > MAX_LINE1_PLAINTEXT_SIZE;
	// Locate the footer without materialising lines: the footer is the
	// last non-empty line; a lone "." terminator line after it is skipped,
	// then further trailing blanks are ignored.
	WireLine footerLine;
	const char* footerStart = wireBlock + wireLen;
	{
		const char* cursor = wireBlock + wireLen;
		bool skippedTerminator = false;
		while (cursor > wireBlock)
		{
			WireLine line;
			const char* lineStart = previousLine(cursor, wireBlock, line);
			if (line.len == 0)
			{
				cursor = lineStart;
				continue;
			}
			if (!skippedTerminator && line.len == 1 && line.data[0] == '.')
			{
				skippedTerminator = true;
				cursor = lineStart;
				continue;
			}
			footerLine = line;
			footerStart = lineStart;
			break;
		}
	}
	// footerIndex is the footer's zero-based physical index: count the
	// lines before it (O(1) memory; the block is already bounded).
	size_t footerIndex = 0;
	for (const char* scan = wireBlock; scan < footerStart;)
	{
		WireLine line;
		scan = scanLine(scan, footerStart, line);
		++footerIndex;
	}
	if (footerIndex < 2)
	{
		outCleanBlock.clear();
		// A degenerate block whose only candidate footer is an oversized
		// Line 2: the line-count check fires first, but the oversized
		// line is a local failure, so keep its classification.
		m_lastErrorFatal = secondOversized;
		return Status::Error;
	}

	std::vector<uint8_t> restored;
	uint32_t line1SegmentIndex = 0;
	if (DecryptControlLine(reinterpret_cast<const uint8_t*>(firstLine.data), firstLine.len,
		0, 1, true, restored, &outLine1Salt, &line1SegmentIndex) != Status::Ok ||
		restored.size() < 8 || memcmp(restored.data(), "=ybegin ", 8) != 0 ||
		line1SegmentIndex == 0)
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}

	const std::string begin(restored.begin(), restored.end());
	outCleanBlock.append(begin).append(firstLine.ending);
	WireLine secondLine;
	const char* afterSecond = scanLine(afterFirst, wireBlock + wireLen, secondLine);
	// Control Encryption Standard v1.2 §5: restore physical line 2 first.
	if (DecryptControlLine(reinterpret_cast<const uint8_t*>(secondLine.data), secondLine.len,
		line1SegmentIndex, 2, false, restored) != Status::Ok)
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}
	const bool multipart = restored.size() >= 7 && memcmp(restored.data(), "=ypart ", 7) == 0;
	const size_t nameIndex = begin.find(" name=");
	const size_t partIndex = begin.find(" part=");
	const bool hasPart = partIndex != std::string::npos && (nameIndex == std::string::npos || partIndex < nameIndex);
	if (multipart != hasPart)
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}
	const char* bodyCursor = afterSecond;
	if (multipart)
	{
		outCleanBlock.append(reinterpret_cast<const char*>(restored.data()), restored.size()).append(secondLine.ending);
		if (footerIndex < 3)
		{
			outCleanBlock.clear();
			outLine1Salt.clear();
			return Status::Error;
		}
		WireLine thirdLine;
		bodyCursor = scanLine(afterSecond, wireBlock + wireLen, thirdLine);
		if (DecryptControlLine(reinterpret_cast<const uint8_t*>(thirdLine.data), thirdLine.len,
			line1SegmentIndex, 3, false, restored) != Status::Ok)
		{
			outCleanBlock.clear();
			outLine1Salt.clear();
			return Status::Error;
		}
	}

	YEncryptionHeader header;
	if (!ParseYEncryption(reinterpret_cast<const char*>(restored.data()), restored.size(), header))
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}

	// Enforce Dual-Bootstrap Agreement
	if (header.salt != outLine1Salt || header.segmentIndex != line1SegmentIndex)
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}

	// Stream the body verbatim: every middle line is still checked for a
	// smuggled =yencryption line, but no per-line storage is kept.
	for (const char* scan = bodyCursor; scan < footerStart;)
	{
		WireLine line;
		const char* next = scanLine(scan, footerStart, line);
		if (line.len >= 13 && memcmp(line.data, "=yencryption ", 13) == 0)
		{
			outCleanBlock.clear();
			outLine1Salt.clear();
			return Status::Error;
		}
		outCleanBlock.append(line.data, line.len).append(line.ending);
		scan = next;
	}

	if (DecryptControlLine(reinterpret_cast<const uint8_t*>(footerLine.data), footerLine.len,
		line1SegmentIndex, static_cast<uint32_t>(footerIndex + 1), false, restored) != Status::Ok ||
		restored.size() < 5 || memcmp(restored.data(), "=yend", 5) != 0)
	{
		outCleanBlock.clear();
		outLine1Salt.clear();
		return Status::Error;
	}
	outCleanBlock.append(reinterpret_cast<const char*>(restored.data()), restored.size()).append(footerLine.ending);
	if (outHeader)
	{
		*outHeader = std::move(header);
	}
	return Status::Ok;
}
