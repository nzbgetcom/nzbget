/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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


#ifndef DEOBFUSCATION_H
#define DEOBFUSCATION_H

#include <string>
#include <array>
#include <regex>

namespace Deobfuscation
{
	inline const std::array<std::regex, 11> HASHED_RELEASES_REGEXES{
		std::regex{ "[0-9a-f.]{16}" },
		std::regex{ "^[0-9a-zA-Z]{24,}" },
		std::regex{ "^[a-z0-9]{24}$" },
		std::regex{ "^abc$" },
		std::regex{ "^abc[-_. ]xyz" },
		std::regex{ "^[A-Z]{11,}[0-9]{3}$" },
		std::regex{ "^Backup_[0-9]{5,}S[0-9]{2}-[0-9]{2}$" },
		std::regex{ "^123$" },
		std::regex{ "^b00bs$" },
		std::regex{ "^[0-9]{6}_[0-9]{2}$" },
	};

	bool IsStronglyObfuscated(const std::string& str) noexcept;
	std::string Deobfuscate(const std::string& str) noexcept;
}

#endif
