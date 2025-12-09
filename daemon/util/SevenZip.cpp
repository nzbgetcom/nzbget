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
#ifdef _WIN32
	const auto tool = Utf8::WideToUtf8(m_tool.wstring()).value_or(m_tool.string());
	const auto outputDir =
		"-o" + Utf8::WideToUtf8(m_outputDir.wstring()).value_or(m_outputDir.string());
	const auto archive = Utf8::WideToUtf8(m_archive.wstring()).value_or(m_archive.string());
#else
	const auto tool = m_tool.string();
	const auto outputDir = "-o" + m_outputDir.string();
	const auto archive = m_archive.string();
#endif

	args.push_back(tool.c_str());
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

	const std::string password = MakePassword();

	args.push_back(password.c_str());
	args.push_back(outputDir.c_str());
	args.push_back(archive.c_str());

	return args;
}

bool SevenZip::IsSupported(const boost::filesystem::path& path)
{
	if (!path.has_filename() || !path.has_extension()) return false;

	auto filename = path.filename().string();
	std::transform(filename.begin(), filename.end(), filename.begin(),
				   [](auto c) { return std::tolower(c); });
	const static std::array<std::string_view, 9> formats{".7z", ".zip", ".7z.001", ".tar", ".gz",
														 ".bz", ".bz2", ".tgz",	   ".txz"};

	return std::any_of(formats.cbegin(), formats.cend(), [&](auto ext)
					   { return Util::EndsWith(filename.c_str(), ext.data(), false); });
}

Result SevenZip::DecodeExitCode(int ec) const
{
	switch (ec)
	{
		case ExitCode::Success:
			return {true, ""};
		case ExitCode::Warning:
			return {false,
					"Completed with warnings. Some files may have been skipped because they were "
					"in use"};
		case ExitCode::FatalError:
			return {false,
					"A fatal error occurred. Check for permission issues, a corrupt archive, "
					"password, disk space"};
		case ExitCode::CmdLineError:
			return {false, "Command line error"};
		case ExitCode::NotEnoughMemoryError:
			return {false, "Not enough memory for operation"};
		case ExitCode::CanceledByUser:
			return {false, "User stopped the process"};
		default:
			return {false, "Unknown 7-Zip error"};
	};
}
