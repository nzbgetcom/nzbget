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
#include "Unpack.h"
#include "ExtensionLoader.h"
#include "ExtensionManager.h"

#ifdef _WIN32
#include "Utf8.h"
#endif

namespace fs = boost::filesystem;

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

	std::pair<WebDownloader::EStatus, fs::path>
	Manager::DownloadExtension(const std::string& url, const std::string& extName)
	{
		fs::path tmpFileName = g_Options->GetTempDirPath() / (extName + ".tmp.zip");

#ifdef _WIN32
		const auto tmpFileNameStr =
			Utf8::WideToUtf8(tmpFileName.wstring()).value_or(tmpFileName.string());
#else
		const auto tmpFileNameStr = tmpFileName.string();
#endif

		std::unique_ptr<WebDownloader> downloader = std::make_unique<WebDownloader>();
		downloader->SetUrl(url.c_str());
		downloader->SetForce(true);
		downloader->SetRetry(false);
		downloader->SetOutputFilename(tmpFileNameStr.c_str());
		downloader->SetInfoName(extName.c_str());

		WebDownloader::EStatus status = downloader->DownloadWithRedirects(5);
		downloader.reset();

		return std::make_pair(status, std::move(tmpFileName));
	}

	std::optional<std::string>
	Manager::UpdateExtension(const boost::filesystem::path& filename, const std::string& extName)
	{
		std::unique_lock<std::shared_mutex> lock{m_mutex};

		auto extensionIt = GetByName(extName);
		if (extensionIt == std::end(m_extensions))
		{
			return "Update failed: Extension '" + extName + "' not found";
		}

		if (extensionIt->use_count() > 1)
		{
			return "Update failed: Extension '" + extName + "' is currently in use or running";
		}

		const auto deleteExtError = DeleteExtension(*(*extensionIt));
		if (deleteExtError)
		{
			boost::system::error_code ec;
			fs::remove(filename, ec);
			if (ec)
			{
				return "Failed to remove existing extension (Error: " + deleteExtError.value() + 
									") and failed to cleanup temporary file '" + filename.string() + 
									"' (Error: " + ec.message() + ")";
			}

			return deleteExtError;
		}
#ifdef _WIN32
		const auto wRootDir = Utf8::Utf8ToWide((*extensionIt)->GetRootDir());
		if (!wRootDir)
		{
			return std::string("Failed to install ") + extName +
				   ": couldn't convert path to wide string.";
		}
		const auto installExtError = InstallExtension(filename, *wRootDir);
#else
		const auto rootDir = (*extensionIt)->GetRootDir();
		const auto installExtError = InstallExtension(filename, rootDir);
#endif
		if (installExtError)
		{
			return installExtError;
		}

		m_extensions.erase(extensionIt);
		return std::nullopt;
	}

	std::optional<std::string> Manager::InstallExtension(const boost::filesystem::path& file,
														 const boost::filesystem::path& dest)
	{
		try
		{
			const auto extractor =
				Unpack::MakeExtractor(file, dest, "", Unpack::OverwriteMode::Overwrite);

			const auto result = extractor->Extract();
			if (!result.success)
			{
				return "Extraction failed for archive '" + file.string() +
					   "'. Error: " + std::string(result.message);
			}

			boost::system::error_code ec;
			fs::remove(file, ec);
			if (ec)
			{
				return "Extension unpacked, but failed to delete temporary archive '" +
					   file.string() + "': " + ec.message();
			}

			return std::nullopt;
		}
		catch (const std::exception& e)
		{
			return "Extraction of " + file.string() + " failed: " + e.what();
		}
	}

	std::optional<std::string>
	Manager::DeleteExtension(const std::string& name)
	{
		std::unique_lock<std::shared_mutex> lock{m_mutex};

		auto extensionIt = GetByName(name);
		if (extensionIt == std::end(m_extensions))
		{
			return "Deletion failed: Extension '" + name + "' not found";
		}

		if (extensionIt->use_count() > 1)
		{
			return "Deletion failed: Extension '" + name + "' is currently in use";
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

#ifdef _WIN32
		const auto wlocation = Utf8::Utf8ToWide(location);
		if (!wlocation) 
		{
			return std::string("Failed to delete ") + location + ": couldn't convert path to wide string";
		}
		fs::path targetPath(*wlocation);
#else
		fs::path targetPath(location);
#endif

		boost::system::error_code ec;
		fs::remove_all(targetPath, ec);
		if (ec)
		{
			return std::string("Failed to delete ") + location + ": " + ec.message();
		}

		return std::nullopt;
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
