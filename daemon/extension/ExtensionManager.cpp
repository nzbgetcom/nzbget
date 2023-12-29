/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

#include <algorithm>
#include "Util.h"
#include "FileSystem.h"
#include "NString.h"
#include "ExtensionLoader.h"
#include "ExtensionManager.h"

namespace ExtensionManager
{
	Manager::Manager() noexcept
	{
		m_extensions.reserve(4);
	}

	const Extensions& Manager::GetExtensions() const
	{
		return m_extensions;
	}

	bool Manager::LoadExtensions(const IOptions& options)
	{
		if (Util::EmptyStr(options.GetScriptDir()))
		{
			return false;
		}

		Tokenizer tokDir(options.GetScriptDir(), ",;");
		while (const char* extensionDir = tokDir.Next())
		{
			LoadExtensionDir(extensionDir, false);
		}

		// first add all scripts from Extension Order
		std::vector<std::string> extensionOrder;
		Tokenizer tokOrder(options.GetScriptOrder(), ",;");
		while (const char* extensionName = tokOrder.Next())
		{
			extensionOrder.push_back(std::string(extensionName));
		}

		Sort(extensionOrder);
		CreateTasks(options);
		m_extensions.shrink_to_fit();

		return true;
	}

	void Manager::LoadExtensionDir(const char* directory, bool isSubDir)
	{
		Extension extension;

		if (ExtensionLoader::V2::Load(extension, directory)) {
			if (!ExtensionExists(extension.GetName()))
			{
				m_extensions.push_back(std::move(extension));
			}
			return;
		}

		DirBrowser dir(directory);
		while (const char* filename = dir.Next())
		{
			if (filename[0] == '.' || filename[0] == '_')
				continue;

			BString<1024> location("%s%c%s", directory, PATH_SEPARATOR, filename);
			if (!FileSystem::DirectoryExists(location))
			{

				std::string extensionName = GetExtensionName(filename);
				if (ExtensionExists(extensionName))
				{
					continue;
				}

				extension.SetName(std::move(extensionName));
				extension.SetLocation(*location);
				if (ExtensionLoader::V1::Load(extension))
				{
					m_extensions.push_back(std::move(extension));
				}
			}
			else if (!isSubDir)
			{
				LoadExtensionDir(location, true);
			}
		}
	}

	void Manager::CreateTasks(const IOptions& options) const
	{
		for (const Extension& extension : m_extensions)
		{
			if (!extension.GetSchedulerScript() || Util::EmptyStr(extension.GetTaskTime()))
			{
				continue;
			}
			Tokenizer tok(g_Options->GetExtensions(), ",;");
			while (const char* scriptName = tok.Next())
			{
				if (strcmp(scriptName, extension.GetName()) == 0)
				{
					g_Options->CreateSchedulerTask(
						0,
						extension.GetTaskTime(),
						nullptr,
						Options::scScript,
						extension.GetName()
					);
					break;
				}
			}
		}
	}

	bool Manager::ExtensionExists(const std::string& name) const
	{
		bool result = std::find_if(
			std::cbegin(m_extensions),
			std::cend(m_extensions),
			[&name](const Extension& ext) {
				return name == ext.GetName();
			}
		) != std::end(m_extensions);
		return result;
	}

	void Manager::Sort(const std::vector<std::string>& order)
	{
		auto compare = [](const Extension& a, const Extension& b)
			{
				return strcmp(a.GetName(), b.GetName()) < 0;
			};
		if (order.empty())
		{
			std::sort(
				std::begin(m_extensions),
				std::end(m_extensions),
				compare
			);
			return;
		}


		size_t count = 0;
		for (size_t i = 0; i < order.size(); ++i)
		{
			const std::string& name = order[i];
			auto it = std::find_if(
				std::begin(m_extensions),
				std::end(m_extensions),
				[&name](const Extension& ext)
				{
					return name == ext.GetName();
				}
			);
			if (it != std::end(m_extensions))
			{
				std::iter_swap(std::begin(m_extensions) + count, it);
				++count;
			}
		}

		std::sort(
			std::begin(m_extensions) + count,
			std::end(m_extensions),
			compare
		);
	}

	std::string Manager::GetExtensionName(const std::string& fileName) const
	{
		size_t lastIdx = fileName.find_last_of(".");
		if (lastIdx != std::string::npos)
		{
			return fileName.substr(0, lastIdx);
		}

		return fileName;
	}
}
