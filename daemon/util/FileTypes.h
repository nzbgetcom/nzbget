/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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


#ifndef FILETYPES_H
#define FILETYPES_H

#include <string_view>

/**
 * @brief Centralized utility functions for classifying file extensions.
 */
namespace FileTypes
{
	bool IsSevenZipExt(std::string_view ext);
	bool IsRarExt(std::string_view ext);
	bool IsRarVolumeExt(std::string_view ext);
	bool IsNumericVolumeExt(std::string_view ext);
	bool IsAllDigitsExt(std::string_view ext);
	bool IsArchiveExt(std::string_view ext);
	bool IsDiscStructureExt(std::string_view ext);
	bool IsParityExt(std::string_view ext);
	bool IsVideoExt(std::string_view ext);
	bool IsAudioExt(std::string_view ext);
	bool IsSubtitleExt(std::string_view ext);
	bool IsSampleStem(std::string_view stem);
	bool IsSevenZipFile(std::string_view filename);
	bool IsRarFile(std::string_view filename);
	bool IsArchiveFile(std::string_view filename);
	bool IsSampleFile(std::string_view filename);
}

#endif
