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

#include <string>
#include <sstream>
#include "NewsServerValidator.h"
#include "Options.h"

namespace SystemHealth::NewsServer
{
NewsServerValidator::NewsServerValidator(const ::NewsServer& server)
	: m_server(server), m_name("Server" + std::to_string(server.GetId()))
{
	m_validators.reserve(16);
	m_validators.push_back(std::make_unique<ServerActiveValidator>(server));
	m_validators.push_back(std::make_unique<ServerNameValidator>(server));
	m_validators.push_back(std::make_unique<ServerLevelValidator>(server));
	m_validators.push_back(std::make_unique<ServerOptionalValidator>(server));
	m_validators.push_back(std::make_unique<ServerGroupValidator>(server));
	m_validators.push_back(std::make_unique<ServerHostValidator>(server));
	m_validators.push_back(std::make_unique<ServerPortValidator>(server));
	m_validators.push_back(std::make_unique<ServerUsernameValidator>(server));
	m_validators.push_back(std::make_unique<ServerPasswordValidator>(server));
	m_validators.push_back(std::make_unique<ServerEncryptionValidator>(server));
	m_validators.push_back(std::make_unique<ServerJoinGroupValidator>(server));
	m_validators.push_back(std::make_unique<ServerCipherValidator>(server));
	m_validators.push_back(std::make_unique<ServerConnectionsValidator>(server));
	m_validators.push_back(std::make_unique<ServerRetentionValidator>(server));
	m_validators.push_back(std::make_unique<ServerCertVerificationValidator>(server));
	m_validators.push_back(std::make_unique<ServerIpVersionValidator>(server));
}

Status ServerActiveValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Warning("Server is disabled");

	return Status::Ok();
}

Status ServerNameValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	std::string_view name = m_server.GetName();
	if (name.empty())
		return Status::Info("Name is recommended for clearer logs and troubleshooting");

	return Status::Ok();
}

Status ServerLevelValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	const auto level = m_server.GetLevel();
	if (level < 0 || level > 99) return Status::Error("Level must be between 0 and 99");

	return Status::Ok();
}

Status ServerOptionalValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	if (m_server.GetOptional() && m_server.GetLevel() == 0)
	{
		return Status::Warning(
			"Server is marked as optional but assigned to level 0; this may affect primary "
			"server selection");
	}
	return Status::Ok();
}

Status ServerGroupValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	const auto group = m_server.GetGroup();
	if (group < 0 || group > 99) return Status::Error("Group must be between 0 and 99");

	return Status::Ok();
}

Status ServerHostValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	return RequiredOption(GetName(), m_server.GetHost())
		.And([&]() { return Network::ValidHostname(m_server.GetHost()); });
}

Status ServerPortValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	return Network::ValidPort(m_server.GetPort())
		.And(
			[&]()
			{
				bool tls = m_server.GetTls();
				int port = m_server.GetPort();

				if (tls && (port != 443 && port != 563))
					return Status::Info("Consider using a standard encrypted port (443 or 563)");

				if (!tls && (port != 80 && port != 119))
					return Status::Info("Consider using a standard unencrypted port (80 or 119)");

				return Status::Ok();
			});
}

Status ServerUsernameValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	std::string_view username = m_server.GetUser();
	if (username.empty())
	{
		return Status::Warning("Username is set to empty");
	}
	return Status::Ok();
}

Status ServerPasswordValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();
	return CheckPassword(m_server.GetPassword());
}

Status ServerEncryptionValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	bool tls = m_server.GetTls();
	std::string_view cipher = m_server.GetCipher();

	if (!tls)
	{
		return Status::Warning(
			"TLS is disabled. "
			"Communication with this server will not be encrypted");
	}

	if (tls && !cipher.empty())
	{
		return Status::Info("Using custom cipher suite");
	}

	return Status::Ok();
}

Status ServerJoinGroupValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	int join = m_server.GetJoinGroup();
	if (join < 0 || join > 99)
	{
		return Status::Error("JoinGroup must be between 0 and 99");
	}
	return Status::Ok();
}

Status ServerCipherValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	std::string_view cipher = m_server.GetCipher();
	bool tls = m_server.GetTls();

	if (cipher.empty()) return Status::Ok();

	if (!tls) return Status::Warning("Cipher specified but TLS is disabled");

	return Status::Ok();
}

Status ServerConnectionsValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	int maxConnections = m_server.GetMaxConnections();
	if (maxConnections == 0)
		return Status::Warning("'Connections' is set to '0'. The Server is disabled");

	if (maxConnections < 0 || maxConnections > 999)
		return Status::Error("'Connections' value is invalid. It must be between 0 and 999");

	if (maxConnections < 8)
		return Status::Warning("A low number of connections may impact download performance");

	return Status::Ok();
}

Status ServerRetentionValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	int retention = m_server.GetRetention();
	if (retention > 0 && retention < 100)
	{
		std::stringstream ss;
		ss << "Retention is set to " << retention << " days. "
		   << "This low value may cause downloads to fail";
		return Status::Warning(ss.str());
	}

	if (retention > 10000)	// ~27 years
	{
		std::stringstream ss;
		ss << "Retention (" << retention << ") is unrealistically high "
		   << "and will be limited by the server's actual retention";
		return Status::Info(ss.str());
	}

	return Status::Ok();
}

Status ServerCertVerificationValidator::Validate() const
{
	if (!m_server.GetActive()) return Status::Ok();

	bool tls = m_server.GetTls();
	auto certLevel = m_server.GetCertVerificationLevel();
	if (!tls) return Status::Ok();

	if (certLevel == Options::ECertVerifLevel::cvNone)
		return Status::Warning(
			"Certificate verification is 'None', "
			"making the TLS connection insecure");

	return Status::Ok();
}

Status ServerIpVersionValidator::Validate() const
{
	// Auto: 0,
	// V4: 4,
	// V6: 6
	int ipVersion = m_server.GetIpVersion();
	if (ipVersion == 0 || ipVersion == 4)
		return Status::Ok();

	if (ipVersion == 6)
		return Status::Info("Using IPv6 - ensure your network supports IPv6");

	return Status::Error("Invalid value. Available options are: Auto, IpV4, IpV6");
}

}  // namespace SystemHealth::NewsServer
