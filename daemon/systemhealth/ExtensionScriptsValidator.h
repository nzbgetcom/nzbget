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

#ifndef EXTENSION_SCRIPTS_VALIDATOR_H
#define EXTENSION_SCRIPTS_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"
#include "Validators.h"
#include "ExtensionManager.h"
#include <string_view>

namespace SystemHealth::ExtensionScripts
{
class ExtensionScriptsValidator final : public SectionValidator
{
public:
	explicit ExtensionScriptsValidator(const Options& options,
									   const ExtensionManager::Manager& manager);
	std::string_view GetName() const override { return "ExtensionScripts"; }

private:
	const Options& m_options;
	const ExtensionManager::Manager& m_extensionManager;
};

class ExtensionListValidator final : public Validator
{
public:
	explicit ExtensionListValidator(const Options& options,
									const ExtensionManager::Manager& manager)
		: m_options(options), m_extensionManager(manager)
	{
	}
	std::string_view GetName() const override { return Options::EXTENSIONS; }
	Status Validate() const override;

private:
	const Options& m_options;
	const ExtensionManager::Manager& m_extensionManager;
};

class ScriptOrderValidator : public Validator
{
public:
	explicit ScriptOrderValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SCRIPTORDER; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ScriptPauseQueueValidator : public Validator
{
public:
	explicit ScriptPauseQueueValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SCRIPTPAUSEQUEUE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ShellOverrideValidator : public Validator
{
public:
	explicit ShellOverrideValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SHELLOVERRIDE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class EventIntervalValidator : public Validator
{
public:
	explicit EventIntervalValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::EVENTINTERVAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::ExtensionScripts

#endif
