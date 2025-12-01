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

#ifndef STATUS_H
#define STATUS_H

#include <string>
#include <utility>

#include "Json.h"
#include "Xml.h"

namespace SystemHealth
{
enum class Severity
{
	Ok,
	Info,
	Warning,
	Error
};

class Status final
{
public:
	Status() = default;

	static Status Ok() { return Status(Severity::Ok, ""); }
	static Status Info(std::string message) { return Status(Severity::Info, std::move(message)); }
	static Status Warning(std::string message)
	{
		return Status(Severity::Warning, std::move(message));
	}
	static Status Error(std::string message) { return Status(Severity::Error, std::move(message)); }

	template <typename Fn, typename... Args>
	Status And(Fn&& nextCheck, Args&&... args) const
	{
		if (IsOk()) return std::invoke(std::forward<Fn>(nextCheck), std::forward<Args>(args)...);
		return *this;
	}

	bool IsOk() const { return m_severity == Severity::Ok; }
	bool IsInfo() const { return m_severity == Severity::Info; }
	bool IsWarning() const { return m_severity == Severity::Warning; }
	bool IsError() const { return m_severity == Severity::Error; }

	Severity GetSeverity() const { return m_severity; }

	const std::string& GetMessage() const { return m_message; }

private:
	Status(Severity severity, std::string message)
		: m_message(std::move(message)), m_severity(severity)
	{
	}

	std::string m_message;
	Severity m_severity = Severity::Ok;
};

std::string_view SeverityToStr(Severity severity);

Json::JsonObject ToJson(const Status& status);
Xml::XmlNodePtr ToXml(const Status& status);

}  // namespace SystemHealth

#endif
