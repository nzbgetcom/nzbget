/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef DEOBFUSCATION_H
#define DEOBFUSCATION_H

#include <string>
#include <string_view>

namespace Deobfuscation
{
	bool IsExcessivelyObfuscated(const std::string& str);
	bool IsProbablyLegitimateFilename(std::string_view str);
	std::string Deobfuscate(const std::string& str);
}

#endif
