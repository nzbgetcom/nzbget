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

#include "Options.h"
#include "Status.h"
#include "nzbget.h"
#include <cctype>

#include "SecurityValidator.h"

namespace SystemHealth::Security
{

SecurityValidator::SecurityValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(16);
	m_validators.push_back(std::make_unique<ControlIpValidator>(options));
	m_validators.push_back(std::make_unique<ControlPortValidator>(options));
	m_validators.push_back(std::make_unique<ControlUsernameValidator>(options));
	m_validators.push_back(std::make_unique<ControlPasswordValidator>(options));
	m_validators.push_back(std::make_unique<RestrictedUsernameValidator>(options));
	m_validators.push_back(std::make_unique<RestrictedPasswordValidator>(options));
	m_validators.push_back(std::make_unique<AddUsernameValidator>(options));
	m_validators.push_back(std::make_unique<AddPasswordValidator>(options));
	m_validators.push_back(std::make_unique<AuthorizedIPValidator>(options));
	m_validators.push_back(std::make_unique<FormAuthValidator>(options));
	m_validators.push_back(std::make_unique<SecureControlValidator>(options));
	m_validators.push_back(std::make_unique<SecureCertValidator>(options));
	m_validators.push_back(std::make_unique<SecureKeyValidator>(options));
	m_validators.push_back(std::make_unique<SecurePortValidator>(options));
	m_validators.push_back(std::make_unique<CertCheckValidator>(options));
	m_validators.push_back(std::make_unique<UpdateCheckValidator>(options));

#ifndef _WIN32
	if (options.GetDaemonMode())
	{
		m_validators.push_back(std::make_unique<DaemonUsernameValidator>(options));
	}
	m_validators.push_back(std::make_unique<UmaskValidator>(options));
#endif
}

Status ControlIpValidator::Validate() const
{
	Status s = RequiredOption(Options::CONTROLIP, m_options.GetControlIp());
	if (!s.IsOk()) return s;

	std::string_view ip = m_options.GetControlIp();
	if (ip == "0.0.0.0")
	{
		return Status::Info("'" + std::string(Options::CONTROLIP) +
							"' is '0.0.0.0', allowing connections from any IP. Client mode will "
							"default to 127.0.0.1 to connect");
	}

	if (ip == "127.0.0.1") return Status::Ok();
	if (!ip.empty() && (ip[0] == '/' || ip[0] == '.'))

	{
#ifdef _WIN32
		return Status::Error("Using Unix domain sockets is not supported on Windows");
#else

		return Status::Info("'" + std::string(Options::CONTROLIP) +
							"' is set to a path, activating Unix domain socket mode");
#endif
	}

	return Status::Ok();
}

Status SecureKeyValidator::Validate() const
{
	if (!m_options.GetSecureControl()) return Status::Ok();

	std::string_view keyFile = m_options.GetSecureKey();
	if (m_options.GetSecureControl() && keyFile.empty())
	{
		return Status::Warning("'" + std::string(Options::SECURECERT) + "' is enabled but '" +
							   std::string(Options::SECUREKEY) + "' is empty");
	}

	if (!m_options.GetSecureControl()) return Status::Ok();

	Status s = File::Exists(m_options.GetSecureKey());

	if (!s.IsOk()) return s;

	return File::Readable(m_options.GetSecureKey());
}

Status SecureCertValidator::Validate() const
{
	if (!m_options.GetSecureControl()) return Status::Ok();

	std::string_view certFile = m_options.GetSecureCert();
	if (m_options.GetSecureControl() && certFile.empty())
	{
		return Status::Warning("'" + std::string(Options::SECURECONTROL) + "' is enabled but '" +
							   std::string(Options::SECURECERT) + "' is empty");
	}

	if (!m_options.GetSecureControl()) return Status::Ok();

	Status s = File::Exists(m_options.GetSecureCert());

	if (!s.IsOk()) return s;

	return File::Readable(m_options.GetSecureCert());
}

Status ControlPortValidator::Validate() const
{
	Status s = Network::ValidPort(m_options.GetControlPort());
	if (!s.IsOk()) return s;

	int port = m_options.GetControlPort();
	if (port < 1024)
	{
		return Status::Warning("'" + std::string(Options::CONTROLPORT) +
							   "' is below 1024 and may require root privileges");
	}
	return Status::Ok();
}

Status ControlUsernameValidator::Validate() const
{
	Status s = RequiredOption(Options::CONTROLUSERNAME, m_options.GetControlUsername());
	if (!s.IsOk()) return s;

	std::string_view username = m_options.GetControlUsername();
	if (username == "nzbget")
		return Status::Info("Using default username 'nzbget' is not recommended for security");

	return Status::Ok();
}

Status ControlPasswordValidator::Validate() const
{
	Status s = CheckPassword(m_options.GetControlPassword());
	if (!s.IsOk()) return s;

	std::string_view password = m_options.GetControlPassword();
	if (password == "tegbzn6789")
		return Status::Info("Using default password is not recommended for security");

	return Status::Ok();
}

Status AddUsernameValidator::Validate() const
{
	std::string_view addUser = m_options.GetAddUsername();
	std::string_view controlUser = m_options.GetControlUsername();
	if (!addUser.empty() && controlUser == "nzbget")
	{
		return Status::Warning("'" + std::string(Options::ADDUSERNAME) + "' is enabled while '" +
							   std::string(Options::CONTROLUSERNAME) +
							   "' is still set to the default 'nzbget'");
	}

	return Status::Ok();
}

Status AddPasswordValidator::Validate() const
{
	std::string_view addUser = m_options.GetAddUsername();
	std::string_view addPass = m_options.GetAddPassword();
	if (!addUser.empty() && addPass.empty())
	{
		return Status::Warning("'" + std::string(Options::ADDUSERNAME) + "' is enabled, but '" +
							   std::string(Options::ADDPASSWORD) +
							   "' is empty. This allows add-only access without a password");
	}

	if (addUser.empty() && !addPass.empty())
	{
		return Status::Info("'" + std::string(Options::ADDPASSWORD) + "' is set, but '" +
							std::string(Options::ADDUSERNAME) +
							"' is empty. The add-user is currently disabled");
	}

	return Status::Ok();
}

Status RestrictedUsernameValidator::Validate() const

{
	std::string_view restrictedUser = m_options.GetRestrictedUsername();

	std::string_view controlUser = m_options.GetControlUsername();

	if (!restrictedUser.empty() && controlUser == "nzbget")
	{
		return Status::Warning("'" + std::string(Options::RESTRICTEDUSERNAME) +
							   "' is enabled while '" + std::string(Options::CONTROLUSERNAME) +
							   "' is still set to the default 'nzbget'");
	}

	return Status::Ok();
}

Status RestrictedPasswordValidator::Validate() const
{
	std::string_view restrictedUser = m_options.GetRestrictedUsername();
	std::string_view restrictedPass = m_options.GetRestrictedPassword();
	if (!restrictedUser.empty() && restrictedPass.empty())
	{
		return Status::Warning("'" + std::string(Options::RESTRICTEDUSERNAME) +
							   "' is enabled, but '" + std::string(Options::RESTRICTEDPASSWORD) +
							   "' is empty. This allows restricted access without a password");
	}

	if (restrictedUser.empty() && !restrictedPass.empty())
	{
		return Status::Info("'" + std::string(Options::RESTRICTEDPASSWORD) + "' is set, but '" +
							std::string(Options::RESTRICTEDUSERNAME) +
							"' is empty. The restricted user is currently disabled");
	}

	return Status::Ok();
}

Status SecurePortValidator::Validate() const
{
	if (!m_options.GetSecureControl()) return Status::Ok();

	Status s = Network::ValidPort(m_options.GetSecurePort());
	if (!s.IsOk()) return s;

	int securePort = m_options.GetSecurePort();
	int controlPort = m_options.GetControlPort();

	if (securePort == controlPort)
	{
		return Status::Error("'" + std::string(Options::SECUREPORT) + "' cannot be the same as '" +
							 std::string(Options::CONTROLPORT) + "'");
	}
	return Status::Ok();
}

Status AuthorizedIPValidator::Validate() const { return Status::Ok(); }

Status UpdateCheckValidator::Validate() const { return Status::Ok(); }

Status FormAuthValidator::Validate() const
{
	if (!m_options.GetFormAuth()) return Status::Ok();

	if (!m_options.GetSecureControl())
	{
		return Status::Warning("'" + std::string(Options::FORMAUTH) + "' is enabled but '" +
							   std::string(Options::SECURECONTROL) +
							   "' is disabled. Form "
							   "credentials may be transmitted in plaintext");
	}

	const bool hasAdd = Util::EmptyStr(m_options.GetAddUsername());
	const bool hasRestricted = Util::EmptyStr(m_options.GetRestrictedUsername());
	if (!hasAdd && !hasRestricted)
	{
		return Status::Warning(
			"'" + std::string(Options::FORMAUTH) +
			"' is enabled but no form users are configured. Users cannot log in via forms");
	}

	return Status::Ok();
}

Status SecureControlValidator::Validate() const
{
	if (m_options.GetSecureControl() && m_options.GetSecurePort() == 0)
	{
		return Status::Error("'" + std::string(Options::SECURECONTROL) + "' is enabled but '" +
							 std::string(Options::SECUREPORT) + "' is invalid");
	}
	return Status::Ok();
}

Status CertCheckValidator::Validate() const
{
	if (!m_options.GetCertCheck())
	{
		return Status::Warning("'" + std::string(Options::CERTCHECK) +
							   "' is disabled. Connections to news servers may be insecure");
	}
	return Status::Ok();
}

#ifndef _WIN32
Status DaemonUsernameValidator::Validate() const
{
	if (!m_options.GetDaemonMode()) return Status::Ok();
	std::string_view daemonUser = m_options.GetDaemonUsername();
	if (daemonUser == "root")
		return Status::Warning("'" + std::string(Options::DAEMONUSERNAME) +
							   "' is set to 'root', consider using a non-privileged user");

	return Status::Ok();
}

Status UmaskValidator::Validate() const
{
	int umask = m_options.GetUMask();
	if (umask == 0)
		return Status::Warning("'" + std::string(Options::UMASK) +
							   "' is set to 0, files will be created with full permissions");
	return Status::Ok();
}
#endif

}  // namespace SystemHealth::Security
