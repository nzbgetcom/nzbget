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
#include "Unpack.h"
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

	std::tuple<WebDownloader::EStatus, std::string>
	Manager::DownloadExtension(const std::string& url, const std::string& info)
	{
		BString<1024> tempFileName;
		int num = 1;
		while (num == 1 || FileSystem::FileExists(tempFileName))
		{
			tempFileName.Format("%s%cextension-%i.tmp.zip", g_Options->GetTempDir(), PATH_SEPARATOR, num);
			num++;
		}

		std::unique_ptr<WebDownloader> downloader = std::make_unique<WebDownloader>();
		downloader->SetUrl(url.c_str());
		downloader->SetForce(true);
		downloader->SetRetry(false);
		downloader->SetOutputFilename(tempFileName);
		downloader->SetInfoName(info.c_str());

		// do sync download
		WebDownloader::EStatus status = downloader->DownloadWithRedirects(5);
		downloader.reset();

		return std::tuple<WebDownloader::EStatus, std::string>(status, tempFileName);
	}

	bool Manager::UpdateExtension(const std::string& filename, const std::string& extName)
	{
		auto extensionIt = GetByName(extName);
		if (extensionIt == std::end(m_extensions))
		{
			return false;
		}

		const auto& rootDir = extensionIt->GetRootDir();
		if (DeleteExtension(extName) && InstallExtension(filename, rootDir))
		{
			return true;
		}

		return false;
	}

	bool Manager::InstallExtension(const std::string& filename, const std::string& dest)
	{
		UnpackController unpacker;
		std::string outputDir = "-o" + dest;
		UnpackController::ArgList args = {
			g_Options->GetSevenZipCmd(),
			"x",
			filename.c_str(),
			outputDir.c_str(),
			"-y",
		};
		unpacker.SetArgs(std::move(args));
		int res = unpacker.Execute();
		if (res == 0 && FileSystem::DeleteFile(filename.c_str()))
		{
			return true;
		}
		return false;
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
			LoadExtensionDir(extensionDir, false, extensionDir);
		}

		// first add all scripts from Extension Order
		std::vector<std::string> extensionOrder;
		Tokenizer tokOrder(options.GetScriptOrder(), ",;");
		while (const char* extensionName = tokOrder.Next())
		{
			extensionOrder.push_back(std::string(extensionName));
		}

		Sort(extensionOrder);
		CreateTasks();
		m_extensions.shrink_to_fit();

		return true;
	}

	bool Manager::DeleteExtension(const std::string& name)
	{
		auto extensionIt = GetByName(name);
		if (extensionIt == std::end(m_extensions))
		{
			return false;
		}

		const char* location = extensionIt->GetLocation();

		CString err;
		if (FileSystem::DirectoryExists(location) && FileSystem::DeleteDirectoryWithContent(location, err))
		{
			if (!err.Empty())
			{
				return false;
			}

			m_extensions.erase(extensionIt);
			return true;
		}
		else if (FileSystem::FileExists(location) && FileSystem::DeleteFile(location))
		{
			m_extensions.erase(extensionIt);
			return true;
		}

		return false;
	}

	void Manager::LoadExtensionDir(const char* directory, bool isSubDir, const char* rootDir)
	{
		Extension extension;

		if (ExtensionLoader::V2::Load(extension, directory, rootDir)) {
			if (!Exists(extension.GetName()))
			{
				m_extensions.push_back(std::move(extension));
				return;
			}
		}

		DirBrowser dir(directory);
		while (const char* filename = dir.Next())
		{
			if (filename[0] == '.' || filename[0] == '_')
				continue;

			BString<1024> entry("%s%c%s", directory, PATH_SEPARATOR, filename);
			if (!FileSystem::DirectoryExists(entry))
			{
				std::string name = GetExtensionName(filename);
				if (Exists(name))
				{
					continue;
				}

				const char* location = isSubDir ? directory : *entry;
				extension.SetEntry(*entry);
				extension.SetName(std::move(name));
				if (ExtensionLoader::V1::Load(extension, location, rootDir))
				{
					m_extensions.push_back(std::move(extension));
				}
			}
			else if (!isSubDir)
			{
				LoadExtensionDir(entry, true, rootDir);
			}
		}
	}

	void Manager::CreateTasks() const
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

	bool Manager::Exists(const std::string& name) const
	{
		return GetByName(name) != std::end(m_extensions);
	}

	void Manager::Sort(const std::vector<std::string>& order)
	{
		auto comparator = [](const Extension& a, const Extension& b) -> bool
			{
				return strcmp(a.GetName(), b.GetName()) < 0;
			};
		if (order.empty())
		{
			std::sort(
				std::begin(m_extensions),
				std::end(m_extensions),
				comparator
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
			comparator
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

	Extensions::const_iterator Manager::GetByName(const std::string& name) const
	{
		return std::find_if(
			std::begin(m_extensions),
			std::end(m_extensions),
			[&name](const Extension& ext)
			{
				return ext.GetName() == name;
			}
		);
	}

	void Manager::DeleteAllExtensions()
	{
		m_extensions.clear();
	}
}
