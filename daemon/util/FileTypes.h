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

// 7-Zip-supported archive extensions (.7z, .zip, .tar, .gz, .bz, .bz2, .tgz, .txz, .xz)
bool IsSevenZipExt(std::string_view ext);

// Single-part RAR (.rar)
bool IsRarExt(std::string_view ext);

// Old-style RAR volumes (.r##–.z##)
bool IsRarVolumeExt(std::string_view ext);

// Generic numeric split volumes (.000–.999, or 4 digits e.g. .0000)
bool IsNumericVolumeExt(std::string_view ext);

// Extension consisting entirely of digits, of any length (e.g. ".1", ".15", ".001").
// Unlike IsNumericVolumeExt, this has no length bound - used where the extension is
// already known/suspected to be a split-volume counter of unknown digit count.
bool IsAllDigitsExt(std::string_view ext);

// Any supported archive format (7-Zip-supported, RAR, or RAR/numeric split-volume extensions)
bool IsArchiveExt(std::string_view ext);

// Disc structure files (.vob, .bdmv, .mpls, .mpl, .clpi, .cpi, .bdm, .ifo, .bup, .mts, .m2ts)
bool IsDiscStructureExt(std::string_view ext);

// Checksum/parity files (.par2, .sfv)
bool IsParityExt(std::string_view ext);

// Video containers (.mkv, .mp4, .avi, .mov, .m2ts, .mts, .ts, .m4v, .webm, .flv, .wmv, .divx, .xvid)
bool IsVideoExt(std::string_view ext);

// Audio formats (.mp3, .flac, .aac, .ogg, .wav, .dts, .ac3, .mka, .opus, .wma, .eac3)
bool IsAudioExt(std::string_view ext);

// Subtitle formats (.srt, .sub, .idx, .ass, .ssa, .smi, .sup, .pgs, .vtt)
bool IsSubtitleExt(std::string_view ext);

// Checks if the filename stem indicates a sample file.
bool IsSampleStem(std::string_view stem);

}

#endif
