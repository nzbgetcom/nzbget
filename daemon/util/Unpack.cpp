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

#include "Options.h"
#include "Unpack.h"
#include "Log.h"

namespace Unpack
{
Extractor::~Extractor() = default;

ExtractorBase::ExtractorBase(fs::path tool, fs::path archive,
							 fs::path outputDir, std::string password,
							 OverwriteMode mode)
	: m_tool(std::move(tool)),
	  m_archive(std::move(archive)),
	  m_outputDir(std::move(outputDir)),
	  m_password(std::move(password)),
	  m_mode(mode)
{
}

bool ExtractorBase::Extract()
{
	if (!CheckPrerequisites()) return false;

	auto args = MakeArgs();
	ScriptController executor;
	executor.SetArgs(std::move(args));

	int exitCode = executor.Execute();
	if (executor.IsTerminated())
	{
		error("Unpack: Extraction process for '%s' was interrupted", fs::u8string(m_archive).c_str());
		return false;
	}

	return DecodeExitCode(exitCode);
}

bool ExtractorBase::CheckPrerequisites() const
{
	fs::error_code ec;
	fs::create_directories(m_outputDir, ec);
	if (ec)
	{
		error("Unpack: Failed to create the output directory '%s': %s", 
			  fs::u8string(m_outputDir).c_str(), ec.message().c_str());
		return false;
	}

	return true;
}

std::string ExtractorBase::MakePassword() const
{
	if (m_password.empty()) return "-p-";

#ifdef WIN32
	// the Windows CRT tokenizes one command-line string, so the surrounding
	// quotes are stripped before unrar/7z see the password
	return "-p\"" + m_password + "\"";
#else
	// POSIX ScriptController uses fork+execvp with an argv array (no shell):
	// literal quotes would become part of the password and fail extraction
	return "-p" + m_password;
#endif
}

bool IsArchive(const fs::path& file)
{
	return SevenZip::IsSupported(file) || Unrar::IsSupported(file);
}

ExtractorPtr MakeExtractor(fs::path archive, fs::path outputDir,
						   std::string password, OverwriteMode mode)
{
	const std::string archiveName = fs::u8string(archive.filename());

	auto validateTool = [&archiveName](const fs::path& toolPath, std::string_view optionName) -> std::optional<fs::path>
	{
		if (toolPath.empty())
		{
			error("Unpack: Could not process '%s': %s is not configured", archiveName.c_str(), std::string(optionName).c_str());
			return std::nullopt;
		}

		fs::error_code ec;
		bool exists = fs::exists(toolPath, ec);
		if (ec)
		{
			error("Unpack: Could not process '%s': Failed to access '%s': %s", archiveName.c_str(), fs::u8string(toolPath).c_str(), ec.message().c_str());
			return std::nullopt;
		}
		if (!exists)
		{
			error("Unpack: Could not process '%s': %s doesn't exist", archiveName.c_str(), fs::u8string(toolPath).c_str());
			return std::nullopt;
		}

		return toolPath;
	};

	if (SevenZip::IsSupported(archive))
	{
		const auto tool = validateTool(g_Options->GetSevenZipPath(), Options::SEVENZIPCMD);
		if (!tool) return nullptr;
		return std::make_unique<SevenZip>(*tool, std::move(archive), std::move(outputDir),
										  std::move(password), mode);
	}

	if (Unrar::IsSupported(archive))
	{
		const auto tool = validateTool(g_Options->GetUnrarPath(), Options::UNRARCMD);
		if (!tool) return nullptr;
		return std::make_unique<Unrar>(*tool, std::move(archive), std::move(outputDir),
									   std::move(password), mode);
	}

	error("Unpack: Could not process '%s': Unsupported archive format: %s", archiveName.c_str(), fs::u8string(archive.extension()).c_str());
	return nullptr;
}
}  // namespace Unpack
