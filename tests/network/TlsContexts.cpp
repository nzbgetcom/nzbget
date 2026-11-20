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

#include "daemon/network/TlsContexts.h"

using namespace network;

namespace
{

constexpr const char* CipherA = "ECDHE-RSA-AES128-GCM-SHA256";
constexpr const char* CipherB = "ECDHE-RSA-AES256-GCM-SHA384";

}

BOOST_AUTO_TEST_SUITE(NetworkTest)

BOOST_AUTO_TEST_CASE(TlsContexts_SameKeyReturnsSameContext)
{
	TlsContexts contexts{{.certCheck = false}};
	auto first = contexts.Get(CertVerification::Strict);
	auto second = contexts.Get(CertVerification::Strict);
	BOOST_REQUIRE(first.has_value());
	BOOST_REQUIRE(second.has_value());
	BOOST_CHECK(first.value() == second.value());
}

BOOST_AUTO_TEST_CASE(TlsContexts_DifferentCipherReturnsDifferentContext)
{
	TlsContexts contexts{{.certCheck = false}};
	auto a = contexts.Get(CertVerification::None, CipherA);
	auto b = contexts.Get(CertVerification::None, CipherB);
	BOOST_REQUIRE(a.has_value());
	BOOST_REQUIRE(b.has_value());
	BOOST_CHECK(a.value() != b.value());
}

BOOST_AUTO_TEST_CASE(TlsContexts_NoneVerificationIgnoresMissingStore)
{
	TlsContexts contexts{{.certStore = "/nonexistent/path/does-not-exist", .certCheck = true}};
	BOOST_CHECK(contexts.Get(CertVerification::None).has_value());
}

BOOST_AUTO_TEST_CASE(TlsContexts_StrictWithoutStoreFails)
{
	TlsContexts contexts{{.certStore = "/nonexistent/path/does-not-exist", .certCheck = true}};
	auto result = contexts.Get(CertVerification::Strict);
	BOOST_REQUIRE(!result.has_value());
	BOOST_CHECK(result.error() == Error::TlsVerifyFailed);
}

BOOST_AUTO_TEST_CASE(TlsContexts_InvalidCipherFails)
{
	TlsContexts contexts{{.certCheck = false}};
	auto result = contexts.Get(CertVerification::None, "NOT-A-REAL-CIPHER");
	BOOST_REQUIRE(!result.has_value());
	BOOST_CHECK(result.error() == Error::LocalFailure);
}

BOOST_AUTO_TEST_CASE(TlsContexts_CertCheckRequiresStore)
{
	TlsContexts contexts{{.certStore = "", .certCheck = true}};
	BOOST_CHECK(!contexts.GetCertStore());
	auto result = contexts.Get(CertVerification::Strict);
	BOOST_REQUIRE(!result.has_value());
	BOOST_CHECK(result.error() == Error::TlsVerifyFailed);
}

BOOST_AUTO_TEST_CASE(TlsContexts_CertStoreDirectoryIsLoaded)
{
	const fs::path dir = fs::temp_directory_path() / "nzbget_tls_store_test";
	fs::create_directories(dir);
	TlsContexts contexts{{.certStore = dir, .certCheck = true}};
	BOOST_CHECK(contexts.GetCertStore());
	fs::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(TlsContexts_NoCertCheckHasNoStore)
{
	TlsContexts contexts{{.certCheck = false}};
	BOOST_CHECK(!contexts.GetCertStore());
}

BOOST_AUTO_TEST_SUITE_END()
