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


#include "nzbget.h"

#include "Unpack.h"
#include "Log.h"
#include "ArchiveProcessor.h"

namespace Incoming
{

namespace
{
	const char* LOG_PREFIX = "ArchiveProcessor:";
}

std::optional<std::vector<fs::path>> ArchiveProcessor::Process(const fs::path& archiveFile) const
{
	const std::string filenameStr = fs::u8string(archiveFile.filename());
	info("Extracting '%s'", filenameStr.c_str());

	const auto unpackDir = m_config.unpackDir / fs::make_unique_filename();
	
	if (!Extract(archiveFile, unpackDir))
	{
		return std::nullopt;
	}

	ScanResult scanResult = ScanUnpackDir(unpackDir);

	MoveNzbFiles(scanResult.from, scanResult.to);

	fs::error_code ec;
	fs::remove_all(unpackDir, ec);
	if (ec)
	{
		warn("Failed to clean up temp dir '%s': %s (code: %d)", 
			 fs::u8string(unpackDir).c_str(), ec.message().c_str(), ec.value());
	}

	DisposeArchive(archiveFile, scanResult.nonNzbFiles);

	if (scanResult.to.empty())
	{
		info("Skipped '%s': No NZB files were found inside", filenameStr.c_str());
	}
	else
	{
		info("Successfully extracted %zu NZB file(s) from '%s'", scanResult.to.size(), filenameStr.c_str());
	}
	
	return std::move(scanResult.to);
}

bool ArchiveProcessor::Extract(const fs::path& archiveFile, const fs::path& unpackDir) const
{
	const auto extractor = Unpack::MakeExtractor(archiveFile, unpackDir, "", Unpack::OverwriteMode::Overwrite);
	if (!extractor)
	{
		return false;
	}

	if (!extractor->Extract())
	{
		const std::string archiveName = fs::u8string(archiveFile.filename());
		const std::string brokenDirName = fs::u8string(m_config.brokenDir.filename());
		error("Failed to extract '%s'. Moving to '%s'", archiveName.c_str(), brokenDirName.c_str());

		fs::error_code ec;
		fs::remove_all(unpackDir, ec);
		fs::create_directories(m_config.brokenDir, ec);
		fs::move_file(archiveFile, fs::make_unique_filename(m_config.brokenDir / archiveFile.filename()), ec);
		return false;
	}

	return true;
}

bool ArchiveProcessor::IsNzbFile(const fs::path& filename) const
{
	auto ext = fs::u8string(filename.extension());
	return strcasecmp(ext.c_str(), ".nzb") == 0;
}

ArchiveProcessor::ScanResult ArchiveProcessor::ScanUnpackDir(const fs::path& unpackDir) const
{
	ScanResult result;
	result.nonNzbFiles = 0;
	fs::error_code ec;

	for (auto it = fs::recursive_directory_iterator(unpackDir, fs::directory_options::skip_permission_denied, ec); 
		!ec && it != fs::recursive_directory_iterator(); 
		it.increment(ec))
	{
		if (!fs::is_regular_file(it->path(), ec)) continue;

		const auto& file = it->path();
		const auto& filename = file.filename();
		const std::string filenameStr = fs::u8string(filename);

		if (filenameStr.empty() || filenameStr[0] == '.') continue;

		if (!IsNzbFile(filename))
		{
			debug("%s Found non-NZB file '%s'", LOG_PREFIX, filenameStr.c_str());
			++result.nonNzbFiles;
			continue;
		}

		auto relativePath = fs::relative(file, unpackDir, ec);
		if (ec) continue;

		auto finalDestPath = m_config.destDir / relativePath;
		result.from.push_back(file);
		result.to.push_back(std::move(finalDestPath));
	}
	
	result.from.shrink_to_fit();
	result.to.shrink_to_fit();
	return result;
}

void ArchiveProcessor::MoveNzbFiles(const std::vector<fs::path>& from, const std::vector<fs::path>& to) const
{
	fs::error_code ec;
	for (size_t i = 0; i < from.size(); ++i)
	{
		fs::create_directories(to[i].parent_path(), ec);
		fs::move_file(from[i], to[i], ec); 
		if (ec)
		{
			error("Failed to move NZB '%s': %s", fs::u8string(from[i].filename()).c_str(), ec.message().c_str());
		}
	}
}

void ArchiveProcessor::DisposeArchive(const fs::path& archiveFile, int nonNzbFileCount) const
{
	const std::string filenameStr = fs::u8string(archiveFile.filename());
	const std::string processedDirName = fs::u8string(m_config.processedDir.filename());

	fs::error_code ec;

	if (m_config.action == Options::ENzbDirArchiveAction::Delete)
	{
		if (nonNzbFileCount > 0)
		{
			info("Archive '%s' contains %d non-NZB file(s). Moving to '%s' instead of deleting to prevent data loss", 
				 filenameStr.c_str(), nonNzbFileCount, processedDirName.c_str());
			
			fs::create_directories(m_config.processedDir, ec);
			fs::move_file(archiveFile, fs::make_unique_filename(m_config.processedDir / archiveFile.filename()), ec);
			if (ec)
			{
				warn("Failed to move archive '%s' to '%s': %s (code: %d)", 
					 filenameStr.c_str(), processedDirName.c_str(), ec.message().c_str(), ec.value());
			}
		}
		else
		{
			if (fs::remove(archiveFile, ec) && !ec)
			{
				info("Deleted archive '%s'", filenameStr.c_str());
			}
			else if (ec)
			{
				warn("Failed to delete archive '%s': %s (code: %d)", 
					 filenameStr.c_str(), ec.message().c_str(), ec.value());
			}
		}
	}
	else
	{
		info("Moving archive '%s' to '%s'", filenameStr.c_str(), processedDirName.c_str());
		fs::create_directories(m_config.processedDir, ec);
		fs::move_file(archiveFile, fs::make_unique_filename(m_config.processedDir / archiveFile.filename()), ec);
		if (ec)
		{
			warn("Failed to move archive '%s' to '%s': %s (code: %d)", 
				 filenameStr.c_str(), processedDirName.c_str(), ec.message().c_str(), ec.value());
		}
	}
}

}
