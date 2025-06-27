/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2025 Denis <denis@nzbget.com>
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

#include "Util.h"
#include "FileSystem.h"
#include "NString.h"
#include "Unpack.h"
#include "ExtensionLoader.h"
#include "ExtensionManager.h"

namespace ExtensionManager
{
	void Manager::ForEach(std::function<void(const ExtensionPtr)> callback) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };

		std::for_each(std::cbegin(m_extensions), std::cend(m_extensions), callback);
	}

	std::optional<ExtensionPtr>
	Manager::FindIf(std::function<bool(ExtensionPtr)> comparator) const
	{
		std::shared_lock<std::shared_mutex> lock{ m_mutex };

		auto found = std::find_if(std::cbegin(m_extensions), std::cend(m_extensions), comparator);
		if (found != std::cend(m_extensions))
		{
			return *found;
		}

		return std::nullopt;
	}

	std::pair<WebDownloader::EStatus, std::string>
	Manager::DownloadExtension(const std::string& url, const std::string& extName)
	{
		std::string tmpFileName = std::string(g_Options->GetTempDir()) 
			+ PATH_SEPARATOR 
			+ "extension-" + extName + ".tmp.zip";

		std::unique_ptr<WebDownloader> downloader = std::make_unique<WebDownloader>();
		downloader->SetUrl(url.c_str());
		downloader->SetForce(true);
		downloader->SetRetry(false);
		downloader->SetOutputFilename(tmpFileName.c_str());
		downloader->SetInfoName(extName.c_str());

		WebDownloader::EStatus status = downloader->DownloadWithRedirects(5);
		downloader.reset();

		return std::make_pair(status, std::move(tmpFileName));
	}

	std::optional<std::string>
	Manager::UpdateExtension(const std::string& filename, const std::string& extName)
	{
		std::unique_lock<std::shared_mutex> lock{m_mutex};

		auto extensionIt = GetByName(extName);
		if (extensionIt == std::end(m_extensions))
		{
			return "Failed to find " + extName;
		}

		if (extensionIt->use_count() > 1)
		{
			return "Failed to update: " + filename + " is executing";
		}

		const auto deleteExtError = DeleteExtension(*(*extensionIt));
		if (deleteExtError)
		{
			if (!FileSystem::DeleteFile(filename.c_str()))
			{
				return "Failed to delete extension and temp file: " + filename;
			}

			return deleteExtError;
		}
		
		const auto installExtError = InstallExtension(filename, (*extensionIt)->GetRootDir());
		if (installExtError)
		{
			return installExtError;
		}

		m_extensions.erase(extensionIt);
		return std::nullopt;
	}

	std::optional<std::string> 
	Manager::InstallExtension(const std::string& filename, const std::string& dest)
	{
		if (Util::EmptyStr(g_Options->GetSevenZipCmd()))
		{

			return std::string("\"SevenZipCmd\" is not specified");
		}

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
		
		int ec = unpacker.Execute();

		if (ec < 0)
		{
			return "Failed to unpack " + filename + ". Make sure that the path to 7-Zip is valid.";
		}

		if (ec > 0)
		{
			return "Failed to unpack " + filename + ". " + UnpackController::DecodeSevenZipExitCode(ec);
		}

		if (!FileSystem::DeleteFile(filename.c_str()))
		{
			return "Failed to delete temp file: " + filename;
		}

		return std::nullopt;
	}

	std::optional<std::string>
	Manager::DeleteExtension(const std::string& name)
	{
		std::unique_lock<std::shared_mutex> lock{m_mutex};

		auto extensionIt = GetByName(name);
		if (extensionIt == std::end(m_extensions))
		{
			return "Failed to find " + name;
		}

		if (extensionIt->use_count() > 1)
		{
			return "Failed to delete: " + name + " is executing";
		}

		const auto err = DeleteExtension(*(*extensionIt));
		if (err)
		{
			return err;
		}

		m_extensions.erase(extensionIt);
		return std::nullopt;
	}

	std::optional<std::string>
	Manager::LoadExtensions()
	{
		const char* scriptDir = g_Options->GetScriptDir();
		if (Util::EmptyStr(scriptDir))
		{
			return std::string("\"ScriptDir\" is not specified");
		}

		std::unique_lock<std::shared_mutex> lock{m_mutex};

		m_extensions.clear();

		Tokenizer tokDir(scriptDir, ",;");
		while (const char* extensionDir = tokDir.Next())
		{
			LoadExtensionDir(extensionDir, false, extensionDir);
		}

		Sort(g_Options->GetScriptOrder());
		CreateTasks();
		m_extensions.shrink_to_fit();

		return std::nullopt;
	}

	std::optional<std::string>
	Manager::DeleteExtension(const Extension::Script& extension)
	{
		const char* location = extension.GetLocation();

		ptrdiff_t count = std::count_if(
			std::begin(m_extensions),
			std::end(m_extensions),
			[&location](const auto& ext) { return strcmp(location, ext->GetLocation()) == 0; }
		);

		if (count > 1)
		{
			// for backward compatibility, when multiple V1 extensions placed 
			// in the same directory in which case we have to delete an entry file, 
			// not the entire directory.
			location = extension.GetEntry();
		}

		if (FileSystem::DirectoryExists(location))
		{
			CString err;
			if (!FileSystem::DeleteDirectoryWithContent(location, err))
			{
				return std::optional<std::string>(err.Str());
			}

			return std::nullopt;
		}
		else if (FileSystem::FileExists(location) && FileSystem::DeleteFile(location))
		{
			return std::nullopt;
		}

		return std::string("Failed to delete ") + location;
	}
	
	void Manager::LoadExtensionDir(const char* directory, bool isSubDir, const char* rootDir)
	{
		Extension::Script extension;

		if (ExtensionLoader::V2::Load(extension, directory, rootDir)) {
			if (!Exists(extension.GetName()))
			{
				m_extensions.emplace_back(std::make_shared<Extension::Script>(std::move(extension)));
				return;
			}
		}

		DirBrowser dir(directory);
		while (const char* filename = dir.Next())
		{
			if (filename[0] == '.' || filename[0] == '_')
				continue;

			const std::string entry = std::string(directory) + PATH_SEPARATOR + filename;
			if (!FileSystem::DirectoryExists(entry.c_str()))
			{
				std::string name = GetExtensionName(filename);
				if (Exists(name))
				{
					continue;
				}

				const char* location = isSubDir ? directory : entry.c_str();
				extension.SetEntry(entry.c_str());
				extension.SetName(std::move(name));
				if (ExtensionLoader::V1::Load(extension, location, rootDir))
				{
					m_extensions.emplace_back(std::make_shared<Extension::Script>(std::move(extension)));
				}
			}
			else if (!isSubDir)
			{
				LoadExtensionDir(entry.c_str(), true, rootDir);
			}
		}
	}

	void Manager::CreateTasks() const
	{
		for (const auto& extension : m_extensions)
		{
			if (!extension->GetSchedulerScript() || Util::EmptyStr(extension->GetTaskTime()))
			{
				continue;
			}
			Tokenizer tok(g_Options->GetExtensions(), ",;");
			while (const char* scriptName = tok.Next())
			{
				if (strcmp(scriptName, extension->GetName()) == 0)
				{
					g_Options->CreateSchedulerTask(
						0,
						extension->GetTaskTime(),
						nullptr,
						Options::scScript,
						extension->GetName()
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

	void Manager::Sort(const char* orderStr)
	{
		auto comparator = [](const auto& a, const auto& b) -> bool
		{
			return strcmp(a->GetName(), b->GetName()) < 0;
		};

		if (Util::EmptyStr(orderStr))
		{
			std::sort(
				std::begin(m_extensions),
				std::end(m_extensions),
				comparator
			);
			return;	
		}

		std::vector<std::string> order;
		Tokenizer tokOrder(orderStr, ",;");
		while (const char* extName = tokOrder.Next())
		{	
			auto pos = std::find(
				std::begin(order), 
				std::end(order), 
				extName
			);

			if (pos == std::end(order))
			{
				order.push_back(extName);
			}
		}

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
				[&name](const auto& ext)
				{
					return name == ext->GetName();
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
			[&name](const auto& ext)
			{
				return ext->GetName() == name;
			}
		);
	}
}
