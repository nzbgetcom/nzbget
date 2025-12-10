/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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
#include <fstream>

#include "Connection.h"
#include "Options.h"
#include "NewsServerValidator.h"

struct ServerFixture
{
	std::unique_ptr<NewsServer> CreateServer(
		bool active = true, const char* name = "ValidServer", const char* host = "news.example.com",
		int port = 563, bool tls = true, const char* user = "user", const char* pass = "pass",
		int maxConn = 50, int level = 0, int retention = 0, const char* cipher = "",
		int ipVersion = Connection::ipAuto, bool optional = false, int group = 0, int joinGroup = 0,
		unsigned int certLevel = Options::cvStrict)
	{
		return std::make_unique<NewsServer>(1, active, name, host, port, ipVersion, user, pass,
											(bool)joinGroup, tls, cipher, maxConn, retention, level,
											group, optional, certLevel);
	}
};

BOOST_FIXTURE_TEST_SUITE(NewsServerValidatorsSuite, ServerFixture)

BOOST_AUTO_TEST_CASE(TestActive)
{
	auto server = CreateServer(true);
	SystemHealth::NewsServer::ServerActiveValidator v(*server);
	BOOST_CHECK(v.Validate().IsOk());

	server->SetActive(false);
	SystemHealth::Status s = v.Validate();
	BOOST_CHECK(s.IsWarning());
	BOOST_CHECK(s.GetMessage().find("disabled") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(TestHost)
{
	BOOST_CHECK(SystemHealth::NewsServer::ServerHostValidator(
					*CreateServer(true, "MyServer", "news.valid.com"))
					.Validate()
					.IsOk());

	SystemHealth::Status s =
		SystemHealth::NewsServer::ServerHostValidator(*CreateServer(true, "MyServer", ""))
			.Validate();
	BOOST_CHECK(s.IsError());

	BOOST_CHECK(
		SystemHealth::NewsServer::ServerHostValidator(*CreateServer(true, "MyServer", "bad@host!"))
			.Validate()
			.IsError());
}

BOOST_AUTO_TEST_CASE(TestPort)
{
	BOOST_CHECK(
		SystemHealth::NewsServer::ServerPortValidator(*CreateServer(true, "", "", 563, true))
			.Validate()
			.IsOk());

	BOOST_CHECK(
		SystemHealth::NewsServer::ServerPortValidator(*CreateServer(true, "", "", 443, true))
			.Validate()
			.IsOk());

	SystemHealth::Status s1 =
		SystemHealth::NewsServer::ServerPortValidator(*CreateServer(true, "", "", 9000, true))
			.Validate();
	BOOST_CHECK(s1.IsInfo());

	BOOST_CHECK(
		SystemHealth::NewsServer::ServerPortValidator(*CreateServer(true, "", "", 119, false))
			.Validate()
			.IsOk());

	SystemHealth::Status s2 =
		SystemHealth::NewsServer::ServerPortValidator(*CreateServer(true, "", "", 8080, false))
			.Validate();
	BOOST_CHECK(s2.IsInfo());
}

BOOST_AUTO_TEST_CASE(TestCredentials)
{
	auto sValid = CreateServer(true, "", "", 563, true, "user", "pass");
	BOOST_CHECK(SystemHealth::NewsServer::ServerUsernameValidator(*sValid).Validate().IsOk());
	BOOST_CHECK(SystemHealth::NewsServer::ServerPasswordValidator(*sValid).Validate().IsInfo());

	auto sEmptyUser = CreateServer(true, "", "", 563, true, "", "pass");
	BOOST_CHECK(
		SystemHealth::NewsServer::ServerUsernameValidator(*sEmptyUser).Validate().IsWarning());

	auto sEmptyPass = CreateServer(true, "", "", 563, true, "user", "");
	BOOST_CHECK(
		SystemHealth::NewsServer::ServerPasswordValidator(*sEmptyPass).Validate().IsWarning());

	auto sShortPass = CreateServer(true, "", "", 563, true, "user", "123");
	BOOST_CHECK(SystemHealth::NewsServer::ServerPasswordValidator(*sShortPass).Validate().IsInfo());
}

BOOST_AUTO_TEST_CASE(TestConnections)
{
	BOOST_CHECK(SystemHealth::NewsServer::ServerConnectionsValidator(
					*CreateServer(true, "", "", 0, true, "", "", 50))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::NewsServer::ServerConnectionsValidator(
					*CreateServer(true, "", "", 0, true, "", "", 4))
					.Validate()
					.IsWarning());

	BOOST_CHECK(SystemHealth::NewsServer::ServerConnectionsValidator(
					*CreateServer(true, "", "", 0, true, "", "", 0))
					.Validate()
					.IsError());

	BOOST_CHECK(SystemHealth::NewsServer::ServerConnectionsValidator(
					*CreateServer(true, "", "", 0, true, "", "", 1000))
					.Validate()
					.IsError());
}

BOOST_AUTO_TEST_CASE(TestEncryption)
{
	auto s1 = CreateServer(true, "", "", 563, true, "", "", 50, 0, 0, "");
	BOOST_CHECK(SystemHealth::NewsServer::ServerEncryptionValidator(*s1).Validate().IsOk());

	auto s2 = CreateServer(true, "", "", 119, false, "", "", 50, 0, 0, "");
	BOOST_CHECK(SystemHealth::NewsServer::ServerEncryptionValidator(*s2).Validate().IsWarning());

	auto s3 = CreateServer(true, "", "", 563, true, "", "", 50, 0, 0, "AES");
	BOOST_CHECK(SystemHealth::NewsServer::ServerEncryptionValidator(*s3).Validate().IsInfo());

	auto s4 = CreateServer(true, "", "", 119, false, "", "", 50, 0, 0, "AES");
	BOOST_CHECK(SystemHealth::NewsServer::ServerCipherValidator(*s4).Validate().IsWarning());
}

BOOST_AUTO_TEST_CASE(TestRetention)
{
	BOOST_CHECK(SystemHealth::NewsServer::ServerRetentionValidator(
					*CreateServer(true, "", "", 0, true, "", "", 50, 0, 0))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::NewsServer::ServerRetentionValidator(
					*CreateServer(true, "", "", 0, true, "", "", 50, 0, 3000))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::NewsServer::ServerRetentionValidator(
					*CreateServer(true, "", "", 0, true, "", "", 50, 0, 50))
					.Validate()
					.IsWarning());

	BOOST_CHECK(SystemHealth::NewsServer::ServerRetentionValidator(
					*CreateServer(true, "", "", 0, true, "", "", 50, 0, 20000))
					.Validate()
					.IsInfo());
}

BOOST_AUTO_TEST_CASE(TestOptional)
{
	BOOST_CHECK(
		SystemHealth::NewsServer::ServerOptionalValidator(
			*CreateServer(true, "", "", 0, true, "", "", 50, 0, 0, "", Connection::ipAuto, false))
			.Validate()
			.IsOk());

	BOOST_CHECK(
		SystemHealth::NewsServer::ServerOptionalValidator(
			*CreateServer(true, "", "", 0, true, "", "", 50, 1, 0, "", Connection::ipAuto, true))
			.Validate()
			.IsOk());

	BOOST_CHECK(
		SystemHealth::NewsServer::ServerOptionalValidator(
			*CreateServer(true, "", "", 0, true, "", "", 50, 0, 0, "", Connection::ipAuto, true))
			.Validate()
			.IsWarning());
}

BOOST_AUTO_TEST_CASE(TestCertVerification)
{
	BOOST_CHECK(SystemHealth::NewsServer::ServerCertVerificationValidator(
					*CreateServer(true, "", "", 563, true, "", "", 50, 0, 0, "", Connection::ipAuto,
								  false, 0, 0, Options::cvStrict))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::NewsServer::ServerCertVerificationValidator(
					*CreateServer(true, "", "", 563, true, "", "", 50, 0, 0, "", Connection::ipAuto,
								  false, 0, 0, Options::cvNone))
					.Validate()
					.IsWarning());

	BOOST_CHECK(SystemHealth::NewsServer::ServerCertVerificationValidator(
					*CreateServer(true, "", "", 119, false, "", "", 50, 0, 0, "",
								  Connection::ipAuto, false, 0, 0, Options::cvNone))
					.Validate()
					.IsOk());
}

BOOST_AUTO_TEST_SUITE_END()
