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
#include <fstream>
#include <cstring>

namespace
{
	bool MatchesAnyExt(std::string_view ext, std::span<const std::string_view> formats)
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
	static constexpr std::string_view FORMATS[] = {
		".7z", ".zip", ".tar", ".gz", ".bz", ".bz2", ".tgz", ".txz", ".xz"
	};
	return MatchesAnyExt(ext, FORMATS);
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
	static constexpr std::string_view FORMATS[] = {
		".vob", ".bdmv", ".mpls", ".mpl", ".clpi", ".cpi", ".bdm",
		".ifo", ".bup",
		".mts", ".m2ts",
		".aob", ".evo", ".bdjo"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsDiscStructureDir(std::string_view dirname)
{
	auto bare = Basename(dirname);
	static constexpr std::string_view DISC_DIRS[] = {
		"BDMV", "VIDEO_TS", "AUDIO_TS", "HVDVD_TS", "AVCHD", "CERTIFICATE"
	};
	return std::any_of(std::begin(DISC_DIRS), std::end(DISC_DIRS),
		[&](std::string_view dir) { return Util::StrCaseCmp(bare, dir); });
}

bool IsDiscDescriptorExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".cue", ".mds", ".ccd", ".toc"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsDiscImageExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".iso", ".mdf", ".nrg", ".cdi", ".gdi"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsGenericDiscImageExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".bin", ".img"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsClutterDir(std::string_view dirname)
{
	auto bare = Basename(dirname);
	static constexpr std::string_view CLUTTER_DIRS[] = {
		"@eaDir", ".AppleDouble", "__MACOSX", ".Spotlight-V100", ".Trashes"
	};
	return std::any_of(std::begin(CLUTTER_DIRS), std::end(CLUTTER_DIRS),
		[&](std::string_view dir) { return Util::StrCaseCmp(bare, dir); });
}

bool IsClutterFile(std::string_view filename)
{
	auto bare = Basename(filename);
	// AppleDouble resource fork / extended attribute sidecar files (e.g. ._Movie.mkv)
	if (bare.size() > 2 && bare[0] == '.' && bare[1] == '_')
	{
		return true;
	}

	static constexpr std::string_view CLUTTER_FILES[] = {
		".DS_Store", "Thumbs.db", "desktop.ini", "ehthumbs.db"
	};
	return std::any_of(std::begin(CLUTTER_FILES), std::end(CLUTTER_FILES),
		[&](std::string_view file) { return Util::StrCaseCmp(bare, file); });
}

bool IsParityExt(std::string_view ext)
{
	return Util::StrCaseCmp(ext, ".par2") || Util::StrCaseCmp(ext, ".sfv");
}

bool IsVideoExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".mkv", ".mp4", ".avi", ".mov", ".m2ts", ".mts", ".ts",
		".m4v", ".webm", ".flv", ".wmv", ".divx", ".xvid"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsAudioExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".mp3", ".flac", ".aac", ".ogg", ".wav", ".dts", ".ac3",
		".mka", ".opus", ".wma", ".eac3", ".m4a"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsSubtitleExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".srt", ".sub", ".idx", ".ass", ".ssa", ".smi", ".sup", ".pgs", ".vtt"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsNfoExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".nfo", ".info"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsBookExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".epub", ".pdf", ".mobi", ".azw3", ".cbr", ".cbz", ".djvu", ".m4b"
	};
	return MatchesAnyExt(ext, FORMATS);
}

bool IsImageExt(std::string_view ext)
{
	static constexpr std::string_view FORMATS[] = {
		".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".tif", ".tiff"
	};
	return MatchesAnyExt(ext, FORMATS);
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

std::string_view SniffExtension(std::span<const uint8_t> header)
{
	// 1. PDF: %PDF-
	if (header.size() >= 5 && std::memcmp(header.data(), "%PDF-", 5) == 0)
	{
		return ".pdf";
	}

	// 2. EBML container: MKV or WebM (\x1A\x45\xDF\xA3)
	if (header.size() >= 4 && header[0] == 0x1A && header[1] == 0x45 && header[2] == 0xDF && header[3] == 0xA3)
	{
		std::string_view sv(reinterpret_cast<const char*>(header.data()), header.size());
		if (sv.find("webm") != std::string_view::npos)
		{
			return ".webm";
		}
		return ".mkv";
	}

	// 3. MP4 / MOV / M4V / M4A / M4B (ftyp or moov)
	if (header.size() >= 8 && std::memcmp(header.data() + 4, "ftyp", 4) == 0)
	{
		if (header.size() >= 12)
		{
			std::string_view brand(reinterpret_cast<const char*>(header.data() + 8), 4);
			if (brand == "M4V ") return ".m4v";
			if (brand == "M4A ") return ".m4a";
			if (brand == "M4B ") return ".m4b";
			if (brand == "qt  ") return ".mov";
		}
		return ".mp4";
	}
	if (header.size() >= 8 && std::memcmp(header.data() + 4, "moov", 4) == 0)
	{
		return ".mp4";
	}

	// 4. RIFF container: AVI, WAVE, WEBP
	if (header.size() >= 12 && std::memcmp(header.data(), "RIFF", 4) == 0)
	{
		if (std::memcmp(header.data() + 8, "AVI ", 4) == 0) return ".avi";
		if (std::memcmp(header.data() + 8, "WAVE", 4) == 0) return ".wav";
		if (std::memcmp(header.data() + 8, "WEBP", 4) == 0) return ".webp";
	}

	// 5. Images (high-confidence signatures checked before heuristics like MPEG-TS)
	// PNG (\x89PNG\r\n\x1a\n)
	static constexpr uint8_t PNG_MAGIC[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
	if (header.size() >= 8 && std::memcmp(header.data(), PNG_MAGIC, 8) == 0)
	{
		return ".png";
	}

	// GIF (GIF87a or GIF89a; starts with 'G'=0x47, must precede MPEG-TS)
	if (header.size() >= 6 &&
		(std::memcmp(header.data(), "GIF87a", 6) == 0 || std::memcmp(header.data(), "GIF89a", 6) == 0))
	{
		return ".gif";
	}

	// JPEG (\xFF\xD8\xFF)
	if (header.size() >= 3 && header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF)
	{
		return ".jpg";
	}

	// BMP (BM with reserved bytes 6-9 equal to 0)
	if (header.size() >= 14 && header[0] == 'B' && header[1] == 'M' &&
		header[6] == 0 && header[7] == 0 && header[8] == 0 && header[9] == 0)
	{
		return ".bmp";
	}

	// 6. MPEG-TS (Sync byte 0x47 spaced by 188 or 192 bytes; require 3 packets when buffer allows)
	if (header.size() >= 189 && header[0] == 0x47)
	{
		if (header[188] == 0x47 && (header.size() < 377 || header[376] == 0x47))
		{
			return ".ts";
		}
		if (header.size() >= 193 && header[192] == 0x47 && (header.size() < 385 || header[384] == 0x47))
		{
			return ".ts";
		}
	}

	// 7. WMV / WMA / ASF
	static constexpr uint8_t ASF_GUID[8] = { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11 };
	if (header.size() >= 8 && std::memcmp(header.data(), ASF_GUID, 8) == 0)
	{
		// ASF_Video_Media GUID
		static constexpr uint8_t ASF_VIDEO_GUID[16] = {
			0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
		};
		// ASF_Audio_Media GUID
		static constexpr uint8_t ASF_AUDIO_GUID[16] = {
			0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
		};

		auto ContainsGuid = [](std::span<const uint8_t> buf, const uint8_t guid[16]) {
			if (buf.size() < 16) return false;
			return std::search(buf.begin(), buf.end(), guid, guid + 16) != buf.end();
		};

		if (ContainsGuid(header, ASF_VIDEO_GUID)) return ".wmv";
		if (ContainsGuid(header, ASF_AUDIO_GUID)) return ".wma";
		return ".wmv";
	}

	// 7. FLAC
	if (header.size() >= 4 && std::memcmp(header.data(), "fLaC", 4) == 0)
	{
		return ".flac";
	}

	// 8. MP3 (ID3 tag or MPEG audio sync frame)
	if (header.size() >= 3 && std::memcmp(header.data(), "ID3", 3) == 0)
	{
		return ".mp3";
	}
	if (header.size() >= 2 && header[0] == 0xFF && (header[1] & 0xE6) == 0xE2)
	{
		return ".mp3";
	}

	// 9. OGG
	if (header.size() >= 4 && std::memcmp(header.data(), "OggS", 4) == 0)
	{
		return ".ogg";
	}

	// 10. ZIP or EPUB (PK\x03\x04)
	if (header.size() >= 4 && header[0] == 0x50 && header[1] == 0x4B && header[2] == 0x03 && header[3] == 0x04)
	{
		std::string_view sv(reinterpret_cast<const char*>(header.data()), header.size());
		if (sv.find("mimetypeapplication/epub+zip") != std::string_view::npos ||
			sv.find("application/epub+zip") != std::string_view::npos)
		{
			return ".epub";
		}
		return ".zip";
	}

	// 11. MOBI (BOOKMOBI at offset 60)
	if (header.size() >= 68 && std::memcmp(header.data() + 60, "BOOKMOBI", 8) == 0)
	{
		return ".mobi";
	}

	// 12. RAR (Rar!\x1A\x07)
	static constexpr uint8_t RAR_MAGIC[6] = { 'R', 'a', 'r', '!', 0x1A, 0x07 };
	if (header.size() >= 6 && std::memcmp(header.data(), RAR_MAGIC, 6) == 0)
	{
		return ".rar";
	}

	// 17. 7z (7z\xBC\xAF\x27\x1C)
	static constexpr uint8_t SEVENZIP_MAGIC[6] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
	if (header.size() >= 6 && std::memcmp(header.data(), SEVENZIP_MAGIC, 6) == 0)
	{
		return ".7z";
	}

	// 18. Gzip (\x1F\x8B\x08)
	if (header.size() >= 3 && header[0] == 0x1F && header[1] == 0x8B && header[2] == 0x08)
	{
		return ".gz";
	}

	// 19. Bzip2 (BZh)
	if (header.size() >= 3 && header[0] == 'B' && header[1] == 'Z' && header[2] == 'h')
	{
		return ".bz2";
	}

	// 20. XZ (\xFD7zXZ\x00)
	static constexpr uint8_t XZ_MAGIC[6] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };
	if (header.size() >= 6 && std::memcmp(header.data(), XZ_MAGIC, 6) == 0)
	{
		return ".xz";
	}

	// 21. POSIX tar (ustar at offset 257)
	if (header.size() >= 262 && std::memcmp(header.data() + 257, "ustar", 5) == 0)
	{
		return ".tar";
	}

	// 22. Unix compress (\x1F\x9D)
	if (header.size() >= 2 && header[0] == 0x1F && header[1] == 0x9D)
	{
		return ".Z";
	}

	return "";
}

std::string_view SniffExtension(const std::filesystem::path& filePath)
{
	std::error_code ec;
	if (!std::filesystem::is_regular_file(filePath, ec))
	{
		return "";
	}

	std::ifstream file(filePath, std::ios::binary);
	if (!file)
	{
		return "";
	}

	uint8_t buffer[512];
	file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
	std::streamsize bytesRead = file.gcount();
	if (bytesRead <= 0)
	{
		return "";
	}

	return SniffExtension(std::span<const uint8_t>(buffer, static_cast<size_t>(bytesRead)));
}

}
