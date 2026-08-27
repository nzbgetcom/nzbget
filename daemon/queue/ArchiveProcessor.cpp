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

std::optional<std::vector<fs::path>> ArchiveProcessor::Process(const fs::path& archiveFile, const fs::path& destDir) const
{
	const std::string filenameStr = fs::u8string(archiveFile.filename());
	info("%s Extracting '%s'", LOG_PREFIX, filenameStr.c_str());

	const auto unpackDir = m_config.unpackDir / fs::make_unique_filename();
	
	if (!Extract(archiveFile, unpackDir))
	{
		return std::nullopt;
	}

	detail("%s Extraction of '%s' completed", LOG_PREFIX, filenameStr.c_str());

	auto scanResult = ScanUnpackDir(unpackDir);

	if (scanResult.nzbFiles.empty())
	{
		info("%s Skipped '%s': No NZB files were found inside", LOG_PREFIX, filenameStr.c_str());
	}
	else
	{
		info("%s Successfully extracted %zu NZB file(s) (and %d non-NZB file(s)) from '%s'",
			 LOG_PREFIX, scanResult.nzbFiles.size(), scanResult.nonNzbFiles, filenameStr.c_str());
	}

	std::vector<fs::path> from;
	std::vector<fs::path> to;
	from.reserve(scanResult.nzbFiles.size());
	to.reserve(scanResult.nzbFiles.size());

	for (const auto& relPath : scanResult.nzbFiles)
	{
		from.push_back(unpackDir / relPath);
		to.push_back(destDir / relPath);
	}

	int moved = MoveNzbFiles(from, to);

	fs::error_code ec;
	fs::remove_all(unpackDir, ec);
	if (ec)
	{
		warn("%s Failed to clean up temp dir '%s': %s (code: %d)", 
			 LOG_PREFIX, fs::u8string(unpackDir).c_str(), ec.message().c_str(), ec.value());
	}
	else
	{
		detail("%s Cleaned up temporary directory '%s'", LOG_PREFIX, fs::u8string(unpackDir).c_str());
	}

	if (moved > 0)
	{
		info("%s Moved %d NZB file(s) to '%s'", LOG_PREFIX, moved, fs::u8string(destDir).c_str());
	}

	DisposeArchive(archiveFile, scanResult.nonNzbFiles);

	if (moved == 0)
	{
		to.clear();
	}
	return to;
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
		error("%s Failed to extract '%s'. Moving to '%s'", LOG_PREFIX, archiveName.c_str(), brokenDirName.c_str());

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
		fs::file_status status = fs::symlink_status(it->path(), ec); 
		if (ec || !fs::is_regular_file(status)) continue;

		const auto& file = it->path();
		const auto& filename = file.filename();
		const std::string filenameStr = fs::u8string(filename);

		if (filenameStr.empty() || filenameStr[0] == '.') continue;

		if (!IsNzbFile(filename))
		{
			detail("%s Found non-NZB file '%s' in the archive", LOG_PREFIX, filenameStr.c_str());
			++result.nonNzbFiles;
			continue;
		}

		auto relativePath = fs::relative(file, unpackDir, ec);
		if (ec) continue;

		detail("%s Found NZB file '%s' in the archive", LOG_PREFIX, fs::u8string(relativePath).c_str());

		result.nzbFiles.push_back(std::move(relativePath));
	}
	
	result.nzbFiles.shrink_to_fit();
	return result;
}

int ArchiveProcessor::MoveNzbFiles(const std::vector<fs::path>& from, const std::vector<fs::path>& to) const
{
	int moved = 0;
	fs::error_code ec;
	for (size_t i = 0; i < from.size(); ++i)
	{
		detail("%s Moving '%s' to '%s'", LOG_PREFIX,
			   fs::u8string(from[i].filename()).c_str(), fs::u8string(to[i]).c_str());
		fs::create_directories(to[i].parent_path(), ec);
		fs::move_file(from[i], to[i], ec); 
		if (ec)
		{
			error("%s Failed to move NZB '%s': %s", LOG_PREFIX,
				  fs::u8string(from[i].filename()).c_str(), ec.message().c_str());
		}
		else
		{
			++moved;
		}
	}
	return moved;
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
			info("%s Archive '%s' contains %d non-NZB file(s). Moving to '%s' instead of deleting to prevent data loss", 
				 LOG_PREFIX, filenameStr.c_str(), nonNzbFileCount, processedDirName.c_str());
			
			fs::create_directories(m_config.processedDir, ec);
			fs::move_file(archiveFile, fs::make_unique_filename(m_config.processedDir / archiveFile.filename()), ec);
			if (ec)
			{
				warn("%s Failed to move archive '%s' to '%s': %s (code: %d)", 
					 LOG_PREFIX, filenameStr.c_str(), processedDirName.c_str(), ec.message().c_str(), ec.value());
			}
		}
		else
		{
			if (fs::remove(archiveFile, ec) && !ec)
			{
				info("%s Deleted archive '%s'", LOG_PREFIX, filenameStr.c_str());
			}
			else if (ec)
			{
				warn("%s Failed to delete archive '%s': %s (code: %d)", 
					 LOG_PREFIX, filenameStr.c_str(), ec.message().c_str(), ec.value());
			}
		}
	}
	else
	{
		info("%s Moving archive '%s' to '%s'", LOG_PREFIX, filenameStr.c_str(), processedDirName.c_str());
		fs::create_directories(m_config.processedDir, ec);
		fs::move_file(archiveFile, fs::make_unique_filename(m_config.processedDir / archiveFile.filename()), ec);
		if (ec)
		{
			warn("%s Failed to move archive '%s' to '%s': %s (code: %d)", 
				 LOG_PREFIX, filenameStr.c_str(), processedDirName.c_str(), ec.message().c_str(), ec.value());
		}
	}
}

}
