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

/* Shared RAR key-derivation functions extracted from RarVolume so that both
 * archive-header decryption (RarReader) and, in later M3 tasks, stream repair
 * derive byte-identical keys from the same password + salt. Task 1 provides the
 * KDF layer only; Task 2 extends this class with random-access CBC codecs and
 * rar5 password-check verification. */
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
};

#endif
