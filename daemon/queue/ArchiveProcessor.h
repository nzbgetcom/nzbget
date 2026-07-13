/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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


#ifndef ARCHIVE_PROCESSOR_H
#define ARCHIVE_PROCESSOR_H

#include <vector>
#include <optional>
#include "Options.h"
#include "FileSystem.h"

namespace Incoming
{

struct Config
{
	fs::path unpackDir;
	fs::path processedDir;
	fs::path brokenDir;
	Options::ENzbDirArchiveAction action;
};

class ArchiveProcessor final
{
public:
	explicit ArchiveProcessor(Config config) : m_config(std::move(config))
	{}
	std::optional<std::vector<fs::path>> Process(const fs::path& archiveFile, const fs::path& destDir) const;

private:
	struct ScanResult
	{
		std::vector<fs::path> nzbFiles;
		int nonNzbFiles{};
	};

	bool Extract(const fs::path& archiveFile, const fs::path& unpackDir) const;
	bool IsNzbFile(const fs::path& filename) const;
	ScanResult ScanUnpackDir(const fs::path& unpackDir) const;
	int MoveNzbFiles(const std::vector<fs::path>& from, const std::vector<fs::path>& to) const;
	void DisposeArchive(const fs::path& archiveFile, int nonNzbFileCount) const;
	const Config m_config;
};

}

#endif
