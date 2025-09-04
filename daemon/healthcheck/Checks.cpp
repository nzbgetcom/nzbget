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

#include "Checks.h"

namespace fs = boost::filesystem;
using namespace boost::system;

namespace HealthCheck::Checks
{
	Check CheckRequiredOption(std::string_view name, std::string_view value)
	{
		if (value.empty())
		{
			return Check::Error(std::string(name) + " is required and cannot be empty");
		}

		return Check::Ok();
	}

	Check CheckInterDirOption(std::string_view value)
	{
		if (value.empty())
		{
			return Check::Warning("InterDir is set to empty. Using InterDir is recommended for optimal unpack performance");
		}

		return Check::Ok();
	}

	Check CheckValueUnique(std::string_view name, std::string_view value, std::vector<std::pair<std::string_view, std::string_view>> otherValues)
	{
		const auto found = std::find_if(
			otherValues.cbegin(), otherValues.cend(),
			[&](const auto& pair)
			{
				return pair.second == value;
			}
		);

		if (found == otherValues.end())
			return Check::Ok();

		return Check::Warning(std::string(name) + " and " + std::string(found->first) + " are the same that can lead to unexpected behavior");
	}

	Check CheckRequiredDir(std::string_view name, std::string_view path)
	{
		error_code ec;
		if (!fs::exists(path, ec))
		{
			return Check::Error(std::string(name) + " doesn't exist");
		}

		if (!fs::is_directory(path, ec))
		{
			return Check::Error(std::string(name) + " must be directory, not a file");
		}

		return Check::Ok();
	}

	Check CheckOptionalDir(std::string_view name, std::string_view path)
	{
		error_code ec;
		if (!fs::exists(path, ec))
		{
			return Check::Warning(std::string(name) + " doesn't exist");
		}

		if (!fs::is_directory(path, ec))
		{
			return Check::Error(std::string(name) + " is not a directory");
		}

		return Check::Ok();
	}

// 	std::vector<Check> CheckLogFile(std::string_view path, Options::EWriteLog writeLog)
// 	{
// 		std::vector<Check> checks;
// 		if (path.empty())
// 		{
// 			checks.push_back(
// 				Check::Warning(std::string(Options::LOGFILE) + " is set to empty")
// 			);
// 			checks.push_back(
// 				Check::Info("Logging is recommended for effective debugging and troubleshooting")
// 			);
// 			return checks;
// 		}

// 		return checks;
// 	}

// 	namespace File
// 	{
// 		Check Exists(std::string_view path)
// 		{
// 			error_code ec;
// 			if (!fs::exists(path))
// 			{
// 				return Check::Error("File does not exist");
// 			}

// 			return Check::Ok();

// 		}

// 		Check Readable(std::string_view path)
// 		{
// #ifdef _WIN32
// 			std::ifstream file(path);
// 			if (file.is_open())
// 			{
// 				return Check::Ok();
// 			}
// 			return Check::Error(std::string("File is not readable: '") + std::strerror(errno));
// #else
// 			if (access(path.data(), R_OK) == 0)
// 			{
// 				return Check::Ok();
// 			}
// 			return Check::Error(std::string("File is not readable: '") + std::strerror(errno));
// #endif
// 		}

// 		Check Writable(std::string_view path)
// 		{
// #ifdef _WIN32
// 			std::ofstream file(path, std::ios::app);
// 			if (file.is_open())
// 			{
// 				return Check::Ok();
// 			}
// 			return Check::Error(std::string("File is not writable: ") + std::strerror(errno));
// #else
// 			if (access(path.data(), W_OK) == 0)
// 			{
// 				return Check::Ok();
// 			}
// 			return Check::Error(std::string("File is not writable: ") + std::strerror(errno));
// #endif
// 		}

// 		Check Executable(std::string_view path)
// 		{
// #ifdef _WIN32
// 			const fs::path filePath(path);
// 			if (!filePath.has_extension())
// 			{
// 				return Check::Error("File is not executable (no extension)");
// 			}
// 			std::string ext = filePath.extension().string();
// 			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
// 			if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com")
// 			{
// 				return Check::Ok();
// 			}

// 			return Check::Error("File is not executable (extension is '" + ext + "')");
// #else
// 			if (access(path.data(), X_OK) == 0)
// 			{
// 				return Check::Ok();
// 			}
// 			return Check::Error(std::string("File is not executable: ") + std::strerror(errno));
// #endif
// 		}
// 	}

	namespace Directory
	{
		Check Readable(std::string_view name, std::string_view path)
		{
			error_code ec;
			fs::directory_iterator(path, ec);
			if (ec)
			{
				return Check::Error(std::string(name) + " must be readable");
			}

			return Check::Ok();
		}

		Check Writable(std::string_view name, std::string_view path)
		{
			error_code ec;
			const fs::path dirPath(path);
			const fs::path testPath = dirPath / "nzbget_write_test.txt";

			{
				std::ofstream testFile(testPath.string());
				if (!testFile.is_open())
				{
					return Check::Error("Failed to create test file '" + testPath.filename().string() + "': " + std::strerror(errno));
				}

				testFile << "Write test.";
				if (testFile.fail())
				{
					return Check::Error("Failed to write to test file '" + testPath.filename().string() + "': " + std::strerror(errno));
				}
			}

			fs::remove(testPath, ec);
			if (ec)
			{
				return Check::Error("Failed to clean up test file '" + testPath.filename().string() + "': " + ec.message());
			}

			return Check::Ok();
		}
	}
}
