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

#include "Status.h"
#include "nzbget.h"

#include "ExtensionScriptsValidator.h"
#include "Validators.h"
#include "Options.h"
#include "Util.h"

namespace SystemHealth::ExtensionScripts
{
ExtensionScriptsValidator::ExtensionScriptsValidator(const Options& options,
													 const ExtensionManager::Manager& manager)
	: m_options(options), m_extensionManager(manager)
{
	m_validators.reserve(5);
	m_validators.push_back(std::make_unique<ExtensionListValidator>(options, manager));
	m_validators.push_back(std::make_unique<ScriptOrderValidator>(options));
	m_validators.push_back(std::make_unique<ScriptPauseQueueValidator>(options));
	m_validators.push_back(std::make_unique<ShellOverrideValidator>(options));
	m_validators.push_back(std::make_unique<EventIntervalValidator>(options));
}

Status ExtensionListValidator::Validate() const
{
	std::string_view extensions = m_options.GetExtensions();
	if (extensions.empty()) return Status::Ok();

	std::string message;
	Tokenizer tokDir(extensions.data(), ",;");
	while (const char* scriptName = tokDir.Next())
	{
		const auto extension =
			m_extensionManager.FindIf([&](const auto ext) 
			{
				std::string_view name = ext->GetName();
				return name == scriptName; 
			});
		if (!extension)
		{
			if (!message.empty()) message += "; ";
			message += std::string("'") + scriptName + "' doesn't exist";
			continue;
		}

		const auto exists = File::Exists(extension.value()->GetEntry());
		if (!exists.IsOk())
		{
			if (!message.empty()) message += "; ";
			message += exists.GetMessage() + " ";
		}
	}

	if (message.empty()) return Status::Ok();

	return Status::Warning(std::move(message));
}

Status ScriptPauseQueueValidator::Validate() const
{
	return Status::Ok();
}

Status ShellOverrideValidator::Validate() const
{
	std::string_view path = m_options.GetShellOverride();
	if (path.empty()) return Status::Ok();

	std::string message;
	Tokenizer tok(path.data(), ",;");
	while (char* shellover = tok.Next())
	{
		char* shellcmd = strchr(shellover, '=');
		if (shellcmd)
		{
			*shellcmd = '\0';
			++shellcmd;
			const auto exists = File::Exists(shellcmd);
			if (!exists.IsOk())
			{
				if (!message.empty()) message += "; ";
				message += exists.GetMessage() + " ";
				continue;
			}
			const auto exe = File::Executable(shellcmd);
			if (!exe.IsOk())
			{
				if (!message.empty()) message += "; ";
				message += exe.GetMessage() + " ";
			}
		}
	}

	if (message.empty()) return Status::Ok();

	return Status::Warning(std::move(message));
}

Status EventIntervalValidator::Validate() const
{
	int val = m_options.GetEventInterval();
	if (val < -1)
	{
		return Status::Error("'" + std::string(Options::EVENTINTERVAL) +
							 "' cannot be less than -1");
	}

	return Status::Ok();
}

Status ScriptOrderValidator::Validate() const
{
	return Status::Ok();
}
}  // namespace SystemHealth::ExtensionScripts
