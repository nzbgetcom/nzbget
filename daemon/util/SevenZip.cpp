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

#include "Unpack.h"
#include "Util.h"

using namespace Unpack;

/**
 * <Commands>
 *		x : eXtract files with full paths
 * <Switches>
 *		-y : assume Yes on all queries
 *		-ao{a|s|t|u} : set Overwrite mode
 *		-p{Password} : set Password
 *		-o{Directory} : set Output directory
 */
ScriptController::ArgList SevenZip::MakeArgs() const
{
	ScriptController::ArgList args;

	args.push_back(fs::u8string(m_tool).c_str());
	args.push_back("x");
	args.push_back("-y");

	switch (m_mode)
	{
		case OverwriteMode::Skip:
			args.push_back("-aos");
			break;
		case OverwriteMode::Overwrite:
			args.push_back("-aoa");
			break;
		case OverwriteMode::AutoRename:
			args.push_back("-aou");
			break;
	}

	args.push_back(MakePassword().c_str());
	args.push_back(("-o" + fs::u8string(m_outputDir)).c_str());
	args.push_back(fs::u8string(m_archive).c_str());

	return args;
}

bool SevenZip::IsSupported(const fs::path& path)
{
	if (!path.has_filename() || !path.has_extension()) return false;

	auto filename = fs::u8string(path.filename());
	std::transform(filename.begin(), filename.end(), filename.begin(),
				   [](auto c) { return std::tolower(c); });
	const static std::array<std::string_view, 9> formats{".7z", ".zip", ".7z.001", ".tar", ".gz",
														 ".bz", ".bz2", ".tgz",	   ".txz"};

	return std::any_of(formats.cbegin(), formats.cend(), [&](auto ext)
					   { return Util::EndsWith(filename.c_str(), ext.data(), false); });
}

bool SevenZip::DecodeExitCode(int ec) const
{
	switch (ec)
	{
		case ExitCode::Success:
			return true;
		case ExitCode::Warning:
			error("Completed with warnings. Some files may have been skipped because they were in use");
			return false;
		case ExitCode::FatalError:
			error("A fatal error occurred. Check for permission issues, a corrupt archive, password, disk space");
			return false;
		case ExitCode::CmdLineError:
			error("Command line error");
			return false;
		case ExitCode::NotEnoughMemoryError:
			error("Not enough memory for operation");
			return false;
		case ExitCode::CanceledByUser:
			error("User stopped the process");
			return false;
		default:
			error("Unknown 7-Zip error (%d)", ec);
			return false;
	};
}
