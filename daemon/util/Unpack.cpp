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

#include <exception>
#include <sstream>
#include <stdexcept>

#include "Options.h"
#include "Unpack.h"

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

Result ExtractorBase::Extract()
{
	const auto result = CheckPrerequisites();
	if (!result.success) return result;

	auto args = MakeArgs();
	ScriptController executor;
	executor.SetArgs(std::move(args));

	int exitCode = executor.Execute();

	return DecodeExitCode(exitCode);
}

Result ExtractorBase::CheckPrerequisites() const
{
	fs::error_code ec;
	fs::create_directories(m_outputDir, ec);
	if (ec) return {false, "Failed to create the output directory"};

	return {true, ""};
}

std::string ExtractorBase::MakePassword() const
{
	if (m_password.empty()) return "-p-";

	return "-p\"" + m_password + "\"";
}

bool IsArchive(const fs::path& file)
{
	return SevenZip::IsSupported(file) || Unrar::IsSupported(file);
}

ExtractorPtr MakeExtractor(fs::path archive, fs::path outputDir,
						   std::string password, OverwriteMode mode)
{
	auto validateTool = [](const fs::path& toolPath, std::string_view optionName)
	{
		if (toolPath.empty())
			throw std::runtime_error(std::string(optionName) + " is not configured");

		fs::error_code ec;
		bool exists = fs::exists(toolPath, ec);
		if (ec)
			throw std::runtime_error("Failed to access '" + fs::u8string(toolPath) +
									 "': " + ec.message());
		if (!exists) throw std::runtime_error(fs::u8string(toolPath) + " doesn't exist");

		return toolPath;
	};

	if (SevenZip::IsSupported(archive))
	{
		const auto tool = validateTool(g_Options->GetSevenZipPath(), Options::SEVENZIPCMD);
		return std::make_unique<SevenZip>(tool, std::move(archive), std::move(outputDir),
										  std::move(password), mode);
	}

	if (Unrar::IsSupported(archive))
	{
		const auto tool = validateTool(g_Options->GetUnrarPath(), Options::UNRARCMD);
		return std::make_unique<Unrar>(tool, std::move(archive), std::move(outputDir),
									   std::move(password), mode);
	}

	throw std::runtime_error("Unsupported archive format: " + fs::u8string(archive.extension()));
}
}  // namespace Unpack
