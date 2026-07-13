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


#include "nzbget.h"

#include "FileTypes.h"
#include "Util.h"

#include <algorithm>
#include <cctype>

namespace
{
	template <size_t N>
	bool MatchesAnyExt(std::string_view ext, const std::string_view (&formats)[N])
	{
		return std::any_of(std::begin(formats), std::end(formats),
			[&](std::string_view fmt) { return Util::StrCaseCmp(ext, fmt); });
	}
}

namespace FileTypes
{

bool IsSevenZipExt(std::string_view ext)
{
	static constexpr std::string_view formats[] = {
		".7z", ".zip", ".tar", ".gz", ".bz", ".bz2", ".tgz", ".txz", ".xz"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsRarExt(std::string_view ext)
{
	return Util::StrCaseCmp(ext, ".rar");
}

bool IsRarVolumeExt(std::string_view ext)
{
	if (ext.size() < 4 || ext.size() > 5 || ext[0] != '.')
	{
		return false;
	}
	return std::tolower(ext[1]) >= 'r' && std::tolower(ext[1]) <= 'z' &&
		std::all_of(ext.begin() + 2, ext.end(),
			[](unsigned char c) { return std::isdigit(c); });
}

bool IsNumericVolumeExt(std::string_view ext)
{
	if (ext.size() < 4 || ext.size() > 5 || ext[0] != '.')
	{
		return false;
	}
	return std::all_of(ext.begin() + 1, ext.end(),
		[](unsigned char c) { return std::isdigit(c); });
}

bool IsArchiveExt(std::string_view ext)
{
	return IsSevenZipExt(ext) || IsRarExt(ext) ||
		IsRarVolumeExt(ext) || IsNumericVolumeExt(ext);
}

bool IsDiscStructureExt(std::string_view ext)
{
	static constexpr std::string_view formats[] = {
		".vob", ".bdmv", ".mpls", ".mpl", ".clpi", ".cpi", ".bdm",
		".ifo", ".bup",
		".mts", ".m2ts"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsParityExt(std::string_view ext)
{
	return Util::StrCaseCmp(ext, ".par2") || Util::StrCaseCmp(ext, ".sfv");
}

bool IsVideoExt(std::string_view ext)
{
	static constexpr std::string_view formats[] = {
		".mkv", ".mp4", ".avi", ".mov", ".m2ts", ".mts", ".ts",
		".m4v", ".webm", ".flv", ".wmv", ".divx", ".xvid"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsAudioExt(std::string_view ext)
{
	static constexpr std::string_view formats[] = {
		".mp3", ".flac", ".aac", ".ogg", ".wav", ".dts", ".ac3",
		".mka", ".opus", ".wma", ".eac3"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsSubtitleExt(std::string_view ext)
{
	static constexpr std::string_view formats[] = {
		".srt", ".sub", ".idx", ".ass", ".ssa", ".smi", ".sup", ".pgs", ".vtt"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsSampleStem(std::string_view stem)
{
	if (stem.size() < 7)
	{
		return false;
	}
	std::string_view suffix = stem.substr(stem.size() - 7);
	return Util::StrCaseCmp(suffix, "-sample") ||
		   Util::StrCaseCmp(suffix, ".sample") ||
		   Util::StrCaseCmp(suffix, "_sample");
}

}
