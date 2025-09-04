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

#ifndef CHECK_H
#define CHECK_H

#include <string>
#include "Json.h"
#include "Xml.h"

namespace HealthCheck
{
	enum class Status { Ok, Info, Warning, Error };

	class Check final
	{
	public:
		Check() = default;

		static Check Ok()
		{
			return Check(Status::Ok, "");
		}

		static Check Info(std::string message)
		{
			return Check(Status::Info, std::move(message));
		}

		static Check Warning(std::string message)
		{
			return Check(Status::Warning, std::move(message));
		}

		static Check Error(std::string message)
		{
			return Check(Status::Error, std::move(message));
		}

		bool IsOk() const { return m_status == Status::Ok; }
		bool IsInfo() const { return m_status == Status::Info; }
		bool IsWarning() const { return m_status == Status::Warning; }
		bool IsError() const { return m_status == Status::Error; }

		explicit operator bool() const 
		{
			return m_status == Status::Ok;
		}

		Status GetStatus() const { return m_status; }
		const std::string& GetMessage() const { return m_message; }

	private:
		Check(Status status, std::string message)
			: m_status{ status }
			, m_message{ std::move(message) }
		{
		}

		Status m_status;
		std::string m_message;
	};

	std::string_view StatusToStr(Status status);

	Json::JsonObject ToJson(const Check& check);
	Xml::XmlNodePtr ToXml(const Check& check);
}

#endif
