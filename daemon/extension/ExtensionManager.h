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

#ifndef EXTENSION_MANAGER_H
#define EXTENSION_MANAGER_H

#include <vector>
#include <utility>
#include <memory>
#include <shared_mutex>
#include <optional>
#include <functional>
#include "WebDownloader.h"
#include "Options.h"
#include "Extension.h"

namespace ExtensionManager
{
	using ExtensionPtr = std::shared_ptr<const Extension::Script>;
	using Extensions = std::vector<ExtensionPtr>;

	class Manager final
	{
	public:
		Manager() = default;
		~Manager() = default;

		Manager(const Manager&) = delete;
		Manager& operator=(const Manager&) = delete;

		Manager(Manager&&) = delete;
		Manager& operator=(Manager&&) = delete;

		std::optional<std::string> 
		InstallExtension(const std::string& filename, const std::string& dest);

		std::optional<std::string> 
		UpdateExtension(const std::string& filename, const std::string& extName);

		std::optional<std::string>
		DeleteExtension(const std::string& name);

		std::optional<std::string>
		LoadExtensions();

		std::pair<WebDownloader::EStatus, std::string>
		DownloadExtension(const std::string& url, const std::string& info);

		void ForEach(std::function<void(const ExtensionPtr)> callback) const;

		std::optional<ExtensionPtr> 
		FindIf(std::function<bool(ExtensionPtr)> comparator) const;

	private:
		void LoadExtensionDir(const char* directory, bool isSubDir, const char* rootDir);
		void CreateTasks() const;
		Extensions::const_iterator GetByName(const std::string& name) const;
		bool Exists(const std::string& name) const;
		void Sort(const char* order);
		std::string GetExtensionName(const std::string& fileName) const;
		std::optional<std::string>
		DeleteExtension(const Extension::Script& ext);

		Extensions m_extensions;
		mutable std::shared_mutex m_mutex;
	};
}

extern ExtensionManager::Manager* g_ExtensionManager;

#endif
