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

#ifndef EXTENSION_MANAGER_H
#define EXTENSION_MANAGER_H

#include <vector>
#include <utility>
#include <memory>
#include <shared_mutex>
#include <boost/optional.hpp>
#include "WebDownloader.h"
#include "Options.h"
#include "Extension.h"

namespace ExtensionManager
{
	using Extensions = std::vector<std::shared_ptr<Extension::Script>>;

	class Manager
	{
	public:
		Manager() noexcept = default;
		~Manager() noexcept = default;

		Manager(const Manager&) = delete;
		Manager& operator=(const Manager&) = delete;

		Manager(Manager&&) noexcept = delete;
		Manager& operator=(Manager&&) noexcept = delete;

		boost::optional<std::string> 
		InstallExtension(const std::string& filename, const std::string& dest);

		boost::optional<std::string> 
		UpdateExtension(const std::string& filename, const std::string& extName);

		boost::optional<std::string>
		DeleteExtension(const std::string& name);

		boost::optional<std::string>
		LoadExtensions(const IOptions& options);

		std::pair<WebDownloader::EStatus, std::string>
		DownloadExtension(const std::string& url, const std::string& info);
		
		const Extensions& GetExtensions() const;

	private:
		void LoadExtensionDir(const char* directory, bool isSubDir, const char* rootDir);
		void CreateTasks() const;
		Extensions::const_iterator GetByName(const std::string& name) const;
		bool Exists(const std::string& name) const;
		void Sort(const char* order);
		std::string GetExtensionName(const std::string& fileName) const;
		boost::optional<std::string>
		DeleteExtension(const Extension::Script& ext);

		Extensions m_extensions;
		mutable std::shared_timed_mutex m_write;
	};
}

extern ExtensionManager::Manager* g_ExtensionManager;

#endif
