/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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

#include <regex>

#include "Unpack.h"
#include "Util.h"

using namespace Unpack;

/**
 * <Commands>
 * 		x : Extract files with full path
 * <Switches>
 *		-y : Assume Yes on all queries
 *		-ai : Ignore file attributes
 *		-o{+|-|r} : Set the overwrite mode
 *		-p{Password} : set Password
 */
ScriptController::ArgList Unrar::MakeArgs() const
{
	ScriptController::ArgList args;

	args.push_back(fs::u8string(m_tool).c_str());
	args.push_back("x");
	args.push_back("-y");
	args.push_back("-ai");

	switch (m_mode)
	{
		case OverwriteMode::Skip:
			args.push_back("-o-");
			break;
		case OverwriteMode::Overwrite:
			args.push_back("-o+");
			break;
		case OverwriteMode::AutoRename:
			args.push_back("-or");
			break;
	}

	args.push_back(MakePassword().c_str());
	args.push_back(fs::u8string(m_archive).c_str());
	args.push_back(fs::u8string(m_outputDir).c_str());

	return args;
}

bool Unrar::IsSupported(const fs::path& path)
{
	if (!path.has_filename()) return false;

	auto filename = fs::u8string(path.filename());
	static const std::regex part1Rar("\\.part0*1\\.rar$", std::regex_constants::icase);
	if (std::regex_search(filename, part1Rar)) return true;

	std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
	if (filename.find(".part") == std::string::npos &&
		Util::EndsWith(filename.c_str(), ".rar", false))
	{
		return true;
	}

	return false;
}

bool Unrar::DecodeExitCode(int ec) const
{
	switch (ec)
	{
		case ExitCode::Success:
			return true;
		case ExitCode::NonFatalError:
			error("Extraction finished, but some files might be missing or incomplete");
			return false;
		case ExitCode::FatalError:
			error("The process could not start or was interrupted unexpectedly");
			return false;
		case ExitCode::InvalidChecksum:
			error("The archive is damaged. The extracted files are likely corrupt");
			return false;
		case ExitCode::LockedArchive:
			error("This archive is locked and cannot be modified or unpacked");
			return false;
		case ExitCode::WriteError:
			error("Could not write files to the destination. Please check your permissions and disk space");
			return false;
		case ExitCode::FileOpenError:
			error("Couldn't open the archive. The file may be missing or you don't have permission to read it");
			return false;
		case ExitCode::CommandLineError:
			error("An internal program error occurred");
			return false;
		case ExitCode::NotEnoughMemory:
			error("Your computer ran out of memory. Please try closing other applications first");
			return false;
		case ExitCode::FileCreateError:
			error("Couldn't create files in the destination folder. Please check your permissions");
			return false;
		case ExitCode::NoFilesFound:
			error("There were no files inside the archive to extract");
			return false;
		case ExitCode::WrongPassword:
			error("The password you entered was incorrect");
			return false;
		default:
			error("Unknown error %d", ec);
			return false;
	}
}
