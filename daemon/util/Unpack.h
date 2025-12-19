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

#ifndef UNPACK_H
#define UNPACK_H

#include <boost/filesystem.hpp>
#include <memory>
#include <string_view>
#include "ScriptController.h"

#ifdef _WIN32
#include "Utf8.h"
#endif

namespace Unpack
{
struct Result
{
	bool success;
	std::string_view message;
};

enum class OverwriteMode
{
	Skip,
	Overwrite,
	AutoRename
};

class Extractor
{
public:
	virtual Result Extract() = 0;
	virtual ~Extractor();
};

class ExtractorBase : public Extractor
{
public:
	ExtractorBase(boost::filesystem::path tool, boost::filesystem::path archive,
				  boost::filesystem::path outputDir, std::string password, OverwriteMode mode);
	Result Extract() final;

protected:
	virtual ScriptController::ArgList MakeArgs() const = 0;
	virtual std::string MakePassword() const;
	virtual Result DecodeExitCode(int ec) const = 0;

	const boost::filesystem::path m_tool;
	const boost::filesystem::path m_archive;
	const boost::filesystem::path m_outputDir;
	const std::string m_password;
	const OverwriteMode m_mode;

private:
	Result CheckPrerequisites() const;
};

class SevenZip final : public ExtractorBase
{
public:
	enum ExitCode
	{
		Success = 0,
		Warning = 1,
		FatalError = 2,
		CmdLineError = 7,
		NotEnoughMemoryError = 8,
		CanceledByUser = 255
	};

	using ExtractorBase::ExtractorBase;
	SevenZip() = delete;
	SevenZip(const SevenZip&) = delete;
	SevenZip operator=(const SevenZip&) = delete;

	static bool IsSupported(const boost::filesystem::path& path);
private:
	ScriptController::ArgList MakeArgs() const override;
	Result DecodeExitCode(int ec) const override;
};

class Unrar final : public ExtractorBase
{
public:
	using Extensions = std::array<std::string_view, 8>;

	enum ExitCode
	{
		Success = 0,
		NonFatalError = 1,
		FatalError = 2,
		InvalidChecksum = 3,
		LockedArchive = 4,
		WriteError = 5,
		FileOpenError = 6,
		CommandLineError = 7,
		NotEnoughMemory = 8,
		FileCreateError = 9,
		NoFilesFound = 10,
		WrongPassword = 11
	};

	using ExtractorBase::ExtractorBase;
	Unrar() = delete;
	Unrar(const Unrar&) = delete;
	Unrar operator=(const Unrar&) = delete;

	static bool IsSupported(const boost::filesystem::path& path);
private:
	ScriptController::ArgList MakeArgs() const override;
	Result DecodeExitCode(int ec) const override;
};

using ExtractorPtr = std::unique_ptr<Extractor>;
ExtractorPtr MakeExtractor(boost::filesystem::path archive, boost::filesystem::path outputDir,
						   std::string password, OverwriteMode mode) noexcept(false);
bool IsArchive(const boost::filesystem::path& file);
}  // namespace Unpack

#endif
