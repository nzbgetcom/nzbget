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
#ifndef VALIDATORS_H
#define VALIDATORS_H

#include <string_view>
#include <string>
#include <vector>
#include <functional>
#include <boost/filesystem.hpp>
#include "Status.h"

namespace SystemHealth
{
class Validator
{
public:
	virtual ~Validator();
	virtual std::string_view GetName() const = 0;
	virtual Status Validate() const = 0;
};

Status RequiredOption(std::string_view name, std::string_view value);
Status UniquePath(
	std::string_view name, const boost::filesystem::path& path,
	const std::vector<std::pair<std::string_view, const boost::filesystem::path&>>& other);
Status RequiredDir(std::string_view name, const boost::filesystem::path& path);
Status RequiredFile(std::string_view name, const boost::filesystem::path& path);
Status OptionalDir(const boost::filesystem::path& path);
Status CheckPassword(std::string_view password);
Status CheckPositiveNum(std::string_view name, int value);

namespace File
{
Status Exists(const boost::filesystem::path& path);
Status Readable(const boost::filesystem::path& path);
Status Writable(const boost::filesystem::path& path);
Status Executable(const boost::filesystem::path& path);
}  // namespace File

namespace Directory
{
Status Exists(const boost::filesystem::path& path);
Status Readable(const boost::filesystem::path& path);
Status Writable(const boost::filesystem::path& path);
}  // namespace Directory

namespace Network
{
Status ValidHostname(std::string_view hostname);
Status ValidPort(int port);
}  // namespace Network
}  // namespace SystemHealth

#endif
