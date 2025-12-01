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

	const auto scripts = Util::SplitStr(extensions.data(), ",;");
	for (const auto& scriptName : scripts)
	{
		const auto opt = std::string("Extension ") + *scriptName;
		const auto extension =
			m_extensionManager.FindIf([&](const auto ext) { return ext->GetName() == scriptName; });
		if (!extension) return Status::Warning(opt + " doesn't exist");

		const auto entry = (*extension)->GetEntry();
		const auto exe = File::Executable(entry);
		if (!exe.IsOk()) return Status::Warning(opt + " cannot be executed");
	}
	return Status::Ok();
}

Status ScriptPauseQueueValidator::Validate() const
{
	if (m_options.GetScriptPauseQueue())
	{
		std::string_view extensions = m_options.GetExtensions();
		if (extensions.empty())
		{
			return Status::Warning(std::string(Options::SCRIPTPAUSEQUEUE) +
								   " is enabled but no extensions are configured");
		}
	}
	return Status::Ok();
}

Status ShellOverrideValidator::Validate() const
{
	std::string_view path = m_options.GetShellOverride();
	if (path.empty()) return Status::Ok();

	Status exeStatus = File::Executable(path);
	if (!exeStatus.IsOk()) return exeStatus;

	return Status::Ok();
}

Status EventIntervalValidator::Validate() const
{
	int val = m_options.GetEventInterval();
	if (val < -1)
	{
		return Status::Error(std::string(Options::EVENTINTERVAL) + " cannot be less than -1");
	}

	return Status::Ok();
}

Status ScriptOrderValidator::Validate() const
{
	std::string_view order = m_options.GetScriptOrder();
	if (order.empty()) return Status::Ok();

	// If script order is specified but no extensions configured, warn
	std::string_view extensions = m_options.GetExtensions();
	if (extensions.empty())
	{
		return Status::Warning(std::string(Options::SCRIPTORDER) +
							   " is set but no extensions are configured");
	}

	// Basic sanity: token count shouldn't be excessive
	auto tokens = Util::SplitStr(order.data(), ",;");
	if (tokens.size() > 100)
	{
		return Status::Warning(std::string(Options::SCRIPTORDER) + " contains too many entries");
	}

	return Status::Ok();
}
}  // namespace SystemHealth::ExtensionScripts
