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

#ifndef NEWS_SERVER_VALIDATOR_H
#define NEWS_SERVER_VALIDATOR_H

#include "SectionValidator.h"
#include "NewsServer.h"

namespace SystemHealth::NewsServer
{
class NewsServerValidator final : public SectionValidator
{
public:
	explicit NewsServerValidator(const ::NewsServer& server);
	std::string_view GetName() const override { return m_name; }

private:
	const ::NewsServer& m_server;
	const std::string m_name;
};

class ServerActiveValidator final : public Validator
{
public:
	explicit ServerActiveValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Active"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerNameValidator final : public Validator
{
public:
	explicit ServerNameValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Name"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerLevelValidator final : public Validator
{
public:
	explicit ServerLevelValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Level"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerOptionalValidator final : public Validator
{
public:
	explicit ServerOptionalValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Optional"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerGroupValidator final : public Validator
{
public:
	explicit ServerGroupValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Group"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerHostValidator final : public Validator
{
public:
	explicit ServerHostValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Host"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerPortValidator final : public Validator
{
public:
	explicit ServerPortValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Port"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerUsernameValidator final : public Validator
{
public:
	explicit ServerUsernameValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Username"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerPasswordValidator final : public Validator
{
public:
	explicit ServerPasswordValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Password"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerEncryptionValidator final : public Validator
{
public:
	explicit ServerEncryptionValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Encryption"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerJoinGroupValidator final : public Validator
{
public:
	explicit ServerJoinGroupValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "JoinGroup"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerCipherValidator final : public Validator
{
public:
	explicit ServerCipherValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Cipher"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerConnectionsValidator final : public Validator
{
public:
	explicit ServerConnectionsValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Connections"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerRetentionValidator final : public Validator
{
public:
	explicit ServerRetentionValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "Retention"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerCertVerificationValidator final : public Validator
{
public:
	explicit ServerCertVerificationValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "CertVerification"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

class ServerIpVersionValidator final : public Validator
{
public:
	explicit ServerIpVersionValidator(const ::NewsServer& server) : m_server(server) {}

	std::string_view GetName() const override { return "IpVersion"; }
	Status Validate() const override;

private:
	const ::NewsServer& m_server;
};

}  // namespace SystemHealth::NewsServer

#endif
