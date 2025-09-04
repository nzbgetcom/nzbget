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

#ifndef SECURITY_VALIDATOR_H
#define SECURITY_VALIDATOR_H

#include "SectionValidator.h"
#include "Validators.h"
#include "Options.h"

namespace SystemHealth::Security
{

class SecurityValidator final : public SectionValidator
{
public:
	explicit SecurityValidator(const Options& options);
	std::string_view GetName() const override { return "Security"; }
	SectionReport Validate() const override { return SectionValidator::Validate(); }

private:
	const Options& m_options;
};

class ControlIpValidator final : public Validator
{
public:
	explicit ControlIpValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CONTROLIP; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ControlPortValidator final : public Validator
{
public:
	explicit ControlPortValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CONTROLPORT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ControlUsernameValidator final : public Validator
{
public:
	explicit ControlUsernameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CONTROLUSERNAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ControlPasswordValidator final : public Validator
{
public:
	explicit ControlPasswordValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CONTROLPASSWORD; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class SecureCertValidator final : public Validator
{
public:
	explicit SecureCertValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SECURECERT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class SecureKeyValidator final : public Validator
{
public:
	explicit SecureKeyValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SECUREKEY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RestrictedUsernameValidator final : public Validator
{
public:
	explicit RestrictedUsernameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RESTRICTEDUSERNAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RestrictedPasswordValidator final : public Validator
{
public:
	explicit RestrictedPasswordValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RESTRICTEDPASSWORD; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class AddUsernameValidator final : public Validator
{
public:
	explicit AddUsernameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ADDUSERNAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class AddPasswordValidator final : public Validator
{
public:
	explicit AddPasswordValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ADDPASSWORD; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class AuthorizedIPValidator final : public Validator
{
public:
	explicit AuthorizedIPValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::AUTHORIZEDIP; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class FormAuthValidator final : public Validator
{
public:
	explicit FormAuthValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::FORMAUTH; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class SecureControlValidator final : public Validator
{
public:
	explicit SecureControlValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SECURECONTROL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class SecurePortValidator final : public Validator
{
public:
	explicit SecurePortValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SECUREPORT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class CertCheckValidator final : public Validator
{
public:
	explicit CertCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CERTCHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UpdateCheckValidator final : public Validator
{
public:
	explicit UpdateCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::UPDATECHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

#ifndef _WIN32
class DaemonUsernameValidator final : public Validator
{
public:
	explicit DaemonUsernameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DAEMONUSERNAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UmaskValidator final : public Validator
{
public:
	explicit UmaskValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::UMASK; }
	Status Validate() const override;

private:
	const Options& m_options;
};
#endif

}  // namespace SystemHealth::Security

#endif
