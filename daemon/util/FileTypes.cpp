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

namespace
{
	bool MatchesAnyExt(std::string_view ext, std::span<std::string_view> formats)
	{
		if (ext.empty() || ext[0] != '.')
		{
			return false;
		}
		return std::any_of(std::begin(formats), std::end(formats),
			[&](std::string_view fmt) { return Util::StrCaseCmp(ext, fmt); });
	}

	std::string_view Basename(std::string_view path)
	{
		auto pos = path.find_last_of("/\\");
		return pos == std::string_view::npos ? path : path.substr(pos + 1);
	}

	std::string_view StripLastExt(std::string_view filename)
	{
		auto pos = filename.rfind('.');
		if (pos == std::string_view::npos || pos == 0)
		{
			return filename;
		}
		return filename.substr(0, pos);
	}
}

namespace FileTypes
{

bool IsSevenZipExt(std::string_view ext)
{
	static constinit std::string_view formats[] = {
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
	if (ext.size() != 4 || ext[0] != '.')
	{
		return false;
	}
	return std::tolower(static_cast<unsigned char>(ext[1])) >= 'r' &&
		std::tolower(static_cast<unsigned char>(ext[1])) <= 'z' &&
		std::isdigit(static_cast<unsigned char>(ext[2])) &&
		std::isdigit(static_cast<unsigned char>(ext[3]));
}

bool IsAllDigitsExt(std::string_view ext)
{
	if (ext.size() < 2 || ext[0] != '.')
	{
		return false;
	}
	return std::all_of(ext.begin() + 1, ext.end(),
		[](unsigned char c) { return std::isdigit(c); });
}

bool IsNumericVolumeExt(std::string_view ext)
{
	if (ext.size() != 4)
	{
		return false;
	}
	return IsAllDigitsExt(ext);
}

bool IsArchiveExt(std::string_view ext)
{
	return IsSevenZipExt(ext) || IsRarExt(ext) ||
		IsRarVolumeExt(ext) || IsNumericVolumeExt(ext);
}

bool IsDiscStructureExt(std::string_view ext)
{
	static constinit std::string_view formats[] = {
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
	static constinit std::string_view formats[] = {
		".mkv", ".mp4", ".avi", ".mov", ".m2ts", ".mts", ".ts",
		".m4v", ".webm", ".flv", ".wmv", ".divx", ".xvid"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsAudioExt(std::string_view ext)
{
	static constinit std::string_view formats[] = {
		".mp3", ".flac", ".aac", ".ogg", ".wav", ".dts", ".ac3",
		".mka", ".opus", ".wma", ".eac3"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsSubtitleExt(std::string_view ext)
{
	static constinit std::string_view formats[] = {
		".srt", ".sub", ".idx", ".ass", ".ssa", ".smi", ".sup", ".pgs", ".vtt"
	};
	return MatchesAnyExt(ext, formats);
}

bool IsSampleStem(std::string_view stem)
{
	if (Util::StrCaseCmp(stem, "sample"))
	{
		return true;
	}
	if (stem.size() < 7)
	{
		return false;
	}
	std::string_view suffix = stem.substr(stem.size() - 7);
	return Util::StrCaseCmp(suffix, "-sample") ||
		   Util::StrCaseCmp(suffix, ".sample") ||
		   Util::StrCaseCmp(suffix, "_sample");
}

bool IsSevenZipFile(std::string_view filename)
{
	auto bare = Basename(filename);
	auto ext = FileSystem::GetFileExtension(bare).value_or("");
	if (ext.empty())
	{
		return false;
	}

	if (Util::StrCaseCmp(ext, ".001") || Util::StrCaseCmp(ext, ".gz") ||
		Util::StrCaseCmp(ext, ".bz2") || Util::StrCaseCmp(ext, ".xz"))
	{
		auto inner = StripLastExt(bare);
		auto innerExt = FileSystem::GetFileExtension(inner).value_or("");
		if (!innerExt.empty() && IsSevenZipExt(innerExt))
		{
			return true;
		}
	}

	return IsSevenZipExt(ext);
}

bool IsRarFile(std::string_view filename)
{
	auto bare = Basename(filename);
	auto ext = FileSystem::GetFileExtension(bare).value_or("");
	if (ext.empty())
	{
		return false;
	}

	if (IsRarExt(ext) || IsRarVolumeExt(ext))
	{
		return true;
	}

	if (IsNumericVolumeExt(ext))
	{
		auto nested = FileSystem::GetFileExtension(StripLastExt(bare)).value_or("");
		if (!nested.empty() && IsRarExt(nested))
		{
			return true;
		}
	}

	return false;
}

bool IsArchiveFile(std::string_view filename)
{
	return IsSevenZipFile(filename) || IsRarFile(filename);
}

bool IsSampleFile(std::string_view filename)
{
	auto bare = Basename(filename);
	auto stem = StripLastExt(bare);
	return IsSampleStem(stem);
}

}
