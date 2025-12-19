/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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
#ifdef _WIN32
	const auto tool = Utf8::WideToUtf8(m_tool.wstring()).value_or(m_tool.string());
	const auto outputDir = Utf8::WideToUtf8(m_outputDir.wstring()).value_or(m_outputDir.string());
	const auto archive = Utf8::WideToUtf8(m_archive.wstring()).value_or(m_archive.string());
#else
	const auto tool = m_tool.string();
	const auto outputDir = m_outputDir.string();
	const auto archive = m_archive.string();
#endif

	args.push_back(tool.c_str());
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

	const std::string password = MakePassword();

	args.push_back(password.c_str());
	args.push_back(archive.c_str());
	args.push_back(outputDir.c_str());

	return args;
}

bool Unrar::IsSupported(const boost::filesystem::path& path)
{
	if (!path.has_filename()) return false;

	auto filename = path.filename().string();
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

Result Unrar::DecodeExitCode(int ec) const
{
	switch (ec)
	{
		case ExitCode::Success:
			return {true, ""};
		case ExitCode::NonFatalError:
			return {false, "Extraction finished, but some files might be missing or incomplete"};
		case ExitCode::FatalError:
			return {false, "The process could not start or was interrupted unexpectedly"};
		case ExitCode::InvalidChecksum:
			return {false, "The archive is damaged. The extracted files are likely corrupt"};
		case ExitCode::LockedArchive:
			return {false, "This archive is locked and cannot be modified or unpacked"};
		case ExitCode::WriteError:
			return {false,
					"Could not write files to the destination. Please check your permissions and "
					"disk space"};
		case ExitCode::FileOpenError:
			return {false,
					"Couldn't open the archive. The file may be missing or you don't have "
					"permission to read it"};
		case ExitCode::CommandLineError:
			return {false, "An internal program error occurred"};
		case ExitCode::NotEnoughMemory:
			return {false,
					"Your computer ran out of memory. Please try closing other applications first"};
		case ExitCode::FileCreateError:
			return {false,
					"Couldn't create files in the destination folder. Please check your "
					"permissions"};
		case ExitCode::NoFilesFound:
			return {false, "There were no files inside the archive to extract"};
		case ExitCode::WrongPassword:
			return {false, "The password you entered was incorrect"};
		default:
			return {false, "Unknown Unrar error"};
	}
}
