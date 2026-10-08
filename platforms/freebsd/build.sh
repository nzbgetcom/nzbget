#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2024 phnzb <pavel@nzbget.com>
#  Copyright (C) 2026 Denis <denis@nzbget.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

set -o nounset
set -o errexit
set -o pipefail

NZBGET_ROOT=$(cd "$(dirname "$0")/../.." && pwd)

. "$NZBGET_ROOT/unpackers.env"

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

SOURCE_DIR="$NZBGET_ROOT/build/dl"
STAGING_BASE="$NZBGET_ROOT/build/staging"
DIST_BASE="$NZBGET_ROOT/build/dist"
UNPACK_BASE="$NZBGET_ROOT/build/unpack"

mkdir -p "$SOURCE_DIR" "$STAGING_BASE" "$DIST_BASE" "$UNPACK_BASE"

ALL_ARCHS="x86_64 aarch64"
ARCHS=""
TARGETS=""
CONFIG="release"
SUFFIX=""
PRESET_PREFIX="ci-freebsd"
TESTING="no"

USAGE="Usage: bash platforms/freebsd/build.sh [bin|installer|all] [x86_64|aarch64|amd64|arm64|all-arch] [release|debug] [testing]"

for PARAM in "$@"; do
	case $PARAM in
		x86_64|amd64)
			ARCHS=$(echo "$ARCHS x86_64" | xargs)
			;;
		aarch64|arm64)
			ARCHS=$(echo "$ARCHS aarch64" | xargs)
			;;
		all-arch)
			ARCHS="$ALL_ARCHS"
			;;
		release)
			CONFIG="release"
			SUFFIX=""
			PRESET_PREFIX="ci-freebsd"
			;;
		debug)
			CONFIG="debug"
			SUFFIX="-debug"
			PRESET_PREFIX="ci-freebsd-debug"
			;;
		testing)
			TESTING="yes"
			;;
		bin|installer|all)
			TARGETS="$PARAM"
			;;
		*)
			echo "Invalid parameter: $PARAM"
			echo "$USAGE"
			exit 1
			;;
	esac
done

if [ -z "$ARCHS" ]; then
	ARCHS="x86_64"
fi
if [ -z "$TARGETS" ]; then
	TARGETS="all"
fi

VERSION=$(grep -m1 "set(VERSION " "$NZBGET_ROOT/CMakeLists.txt" | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ "$TESTING" = "yes" ]; then
	VERSION_SUFFIX="-testing-${BUILD_DATE:-$(date '+%Y%m%d')}"
fi

BASENAME="nzbget-${VERSION}${VERSION_SUFFIX}"
PLATFORM="freebsd"

echo "=== NZBGet FreeBSD Build ==="
echo "Version:       ${VERSION}${VERSION_SUFFIX}"
echo "Targets:       $TARGETS"
echo "Architectures: $ARCHS"
echo "Config:        $CONFIG"
echo "============================"

short_arch() {
	case $1 in
		x86_64|amd64) echo "x86_64" ;;
		aarch64|arm64) echo "aarch64" ;;
		*) echo "$1" ;;
	esac
}

sed_i() {
	local EXPR=$1
	local FILE=$2
	if sed --version >/dev/null 2>&1; then
		sed -i "$EXPR" "$FILE"
	else
		sed -i '' "$EXPR" "$FILE"
	fi
}

md5_of() {
	local FILE=$1
	if command -v md5sum >/dev/null 2>&1; then
		md5sum "$FILE" | cut -b-32
	else
		md5 -q "$FILE"
	fi
}

size_of() {
	local FILE=$1
	if command -v stat >/dev/null 2>&1; then
		stat -c%s "$FILE" 2>/dev/null || stat -f%z "$FILE" 2>/dev/null
	else
		wc -c < "$FILE" | tr -d ' '
	fi
}

download() {
	local URL=$1
	local DEST=$2
	if [ ! -s "$DEST" ]; then
		echo "Downloading $URL"
		curl -fsSL --retry 3 -o "$DEST.part" "$URL"
		mv "$DEST.part" "$DEST"
	fi
}

download_cacert() {
	local dest=$1
	local sha_file="$dest.sha256"
	if [ ! -s "$dest" ] || [ ! -s "$sha_file" ]; then
		echo "Downloading https://curl.se/ca/cacert.pem and verifying checksum..."
		curl -fsSL --retry 3 -o "$dest.part" "https://curl.se/ca/cacert.pem"
		curl -fsSL --retry 3 -o "$sha_file.part" "https://curl.se/ca/cacert.pem.sha256"

		local expected
		expected=$(awk '{print $1}' "$sha_file.part")
		local actual=""
		if command -v sha256sum >/dev/null 2>&1; then
			actual=$(sha256sum "$dest.part" | awk '{print $1}')
		elif command -v shasum >/dev/null 2>&1; then
			actual=$(shasum -a 256 "$dest.part" | awk '{print $1}')
		else
			echo "ERROR: neither sha256sum nor shasum is available for checksum verification"
			exit 1
		fi

		if [ "$actual" != "$expected" ]; then
			echo "ERROR: SHA256 checksum mismatch for cacert.pem"
			echo "  Expected: $expected"
			echo "  Actual:   $actual"
			rm -f "$dest.part" "$sha_file.part"
			exit 1
		fi

		mv "$dest.part" "$dest"
		mv "$sha_file.part" "$sha_file"
	fi
}

ensure_sysroot() {
	local ARCH=$1
	local SYSROOT_NAME="sysroot"
	if [ "$ARCH" = "aarch64" ]; then
		SYSROOT_NAME="sysroot-aarch64"
	fi
	local SYSROOT_DIR="$NZBGET_ROOT/build/toolchains/freebsd/$SYSROOT_NAME"
	if [ ! -d "$SYSROOT_DIR/usr/include" ]; then
		echo "Preparing FreeBSD sysroot for $ARCH..."
		bash "$NZBGET_ROOT/toolchains/build-freebsd.sh" "$ARCH" 13.0
	fi
}

build_arch() {
	local ARCH=$1
	local SHORT
	SHORT=$(short_arch "$ARCH")
	local PRESET="${PRESET_PREFIX}-${SHORT}"
	local BUILD_DIR="$NZBGET_ROOT/build/$PRESET"
	local SYSROOT_NAME="sysroot"
	if [ "$SHORT" = "aarch64" ]; then
		SYSROOT_NAME="sysroot-aarch64"
	fi
	local SYSROOT_DIR="$NZBGET_ROOT/build/toolchains/freebsd/$SYSROOT_NAME"

	echo "--- Building binary for $ARCH ($CONFIG, preset: $PRESET) ---"
	ensure_sysroot "$SHORT"
	mkdir -p "$BUILD_DIR"

	cmake --preset "$PRESET" -DFREEBSD_ARCH="$SHORT" -DFREEBSD_SYSROOT="$SYSROOT_DIR" -DVERSION_SUFFIX="$VERSION_SUFFIX"
	cmake --build --preset "$PRESET" -j "$JOBS" 2>&1 | tee "$BUILD_DIR/build.log"

	if [ ! -f "$BUILD_DIR/nzbget" ]; then
		echo "ERROR: binary not found at $BUILD_DIR/nzbget"
		exit 1
	fi

	local READELF_BIN=""
	if command -v readelf >/dev/null 2>&1; then
		READELF_BIN="readelf"
	elif command -v llvm-readelf >/dev/null 2>&1; then
		READELF_BIN="llvm-readelf"
	fi

	if [ -n "$READELF_BIN" ]; then
		if "$READELF_BIN" -l "$BUILD_DIR/nzbget" 2>/dev/null | grep -qi "program interpreter"; then
			echo "ERROR: $BUILD_DIR/nzbget is dynamically linked! Fully static executable expected."
			exit 1
		else
			echo "Verified: $BUILD_DIR/nzbget is statically linked."
		fi
	fi

	echo "Binary built successfully: $BUILD_DIR/nzbget"
}

package_bin() {
	local ARCH=$1
	local SHORT
	SHORT=$(short_arch "$ARCH")
	local PRESET="${PRESET_PREFIX}-${SHORT}"
	local BUILD_DIR="$NZBGET_ROOT/build/$PRESET"
	local STAGING_DIR="$STAGING_BASE/bin-$ARCH"
	local TAR_PKG="$DIST_BASE/$BASENAME-bin-$PLATFORM-$SHORT$SUFFIX.tar.gz"

	echo "--- Packaging binary archive for $ARCH ($CONFIG) ---"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR/nzbget"

	cp "$BUILD_DIR/nzbget" "$STAGING_DIR/nzbget/nzbget"
	cp -r "$NZBGET_ROOT/webui" "$STAGING_DIR/nzbget/webui"
	cp "$NZBGET_ROOT/ChangeLog.md" "$STAGING_DIR/nzbget/ChangeLog.txt"
	cp "$NZBGET_ROOT/COPYING" "$STAGING_DIR/nzbget/license.txt"
	local CONF="$STAGING_DIR/nzbget/webui/nzbget.conf.template"
	cp "$NZBGET_ROOT/nzbget.conf" "$CONF"
	sed_i 's|^MainDir=.*|MainDir=${AppDir}/downloads|' "$CONF"
	sed_i 's|^DestDir=.*|DestDir=${MainDir}/completed|' "$CONF"
	sed_i 's|^InterDir=.*|InterDir=${MainDir}/intermediate|' "$CONF"
	sed_i 's|^WebDir=.*|WebDir=${AppDir}/webui|' "$CONF"
	sed_i 's|^ScriptDir=.*|ScriptDir=${AppDir}/scripts|' "$CONF"
	sed_i 's|^LogFile=.*|LogFile=${MainDir}/nzbget.log|' "$CONF"
	sed_i 's|^ConfigTemplate=.*|ConfigTemplate=${AppDir}/webui/nzbget.conf.template|' "$CONF"
	sed_i 's|^AuthorizedIP=.*|AuthorizedIP=127.0.0.1|' "$CONF"

	mkdir -p "$DIST_BASE"
	(cd "$STAGING_DIR" && tar -czf "$TAR_PKG" nzbget)
	echo "Created archive: $TAR_PKG"
}

ensure_unpackers() {
	local UNPACK_TAR="$SOURCE_DIR/freebsd-unpack.tar.gz"
	if [ ! -d "$UNPACK_BASE/lib/x86_64-bsd" ]; then
		download "https://github.com/nzbgetcom/build-files/releases/download/v10.0/freebsd-unpack.tar.gz" "$UNPACK_TAR"
		tar -xzf "$UNPACK_TAR" -C "$UNPACK_BASE"
	fi
}

fetch_unpacker() {
	local REPO=$1 VER=$2 ARCH_TAG=$3
	shift 3
	local TAR="$SOURCE_DIR/$REPO-$ARCH_TAG-v$VER.tar.gz"
	local TMP="$SOURCE_DIR/$REPO-$ARCH_TAG-tmp"
	local URL="https://github.com/nzbgetcom/$REPO/releases/download/v$VER/$REPO-$ARCH_TAG.tar.gz"

	if [ ! -s "$TAR" ]; then
		if ! curl -fsSL --retry 2 -o "$TAR.part" "$URL"; then
			rm -f "$TAR.part"
			echo "ERROR: failed to download $REPO from $URL"
			return 1
		fi
		mv "$TAR.part" "$TAR"
	fi

	rm -rf "$TMP"
	mkdir -p "$TMP"
	tar -xzf "$TAR" -C "$TMP"
	local ITEM
	for ITEM in "$@"; do
		if [ -f "$TMP/${ITEM%%:*}" ]; then
			cp "$TMP/${ITEM%%:*}" "${ITEM#*:}"
			chmod +x "${ITEM#*:}"
		fi
	done
	rm -rf "$TMP"
	return 0
}

package_installer() {
	echo "--- Packaging self-extracting .run installer ($CONFIG) ---"
	local STAGING_DIR="$STAGING_BASE/installer"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR/nzbget"

	ensure_unpackers

	local DISTARCHS=""
	for ARCH in $ARCHS; do
		local SHORT
		SHORT=$(short_arch "$ARCH")
		local TAR_PKG="$DIST_BASE/$BASENAME-bin-$PLATFORM-$SHORT$SUFFIX.tar.gz"
		if [ ! -f "$TAR_PKG" ]; then
			# Fallback: look for any matching testing package for this architecture if date differed
			local CANDIDATE
			CANDIDATE=$(find "$DIST_BASE" -maxdepth 1 -name "nzbget-${VERSION}-*-bin-${PLATFORM}-${SHORT}${SUFFIX}.tar.gz" 2>/dev/null | head -n 1)
			if [ -n "$CANDIDATE" ] && [ -f "$CANDIDATE" ]; then
				echo "Notice: using matched package archive: $CANDIDATE"
				TAR_PKG="$CANDIDATE"
			else
				echo "ERROR: required package archive not found: $TAR_PKG"
				exit 1
			fi
		fi
		tar -xzf "$TAR_PKG" -C "$STAGING_DIR"
		mv "$STAGING_DIR/nzbget/nzbget" "$STAGING_DIR/nzbget/nzbget-$SHORT"

		# Unpackers for FreeBSD (single unrar and 7za per architecture)
		local ARCH_TAG="${SHORT}-bsd"
		if ! fetch_unpacker unrar "$UNRAR_VERSION" "$ARCH_TAG" "unrar:$STAGING_DIR/nzbget/unrar-$SHORT" "license.txt:$STAGING_DIR/nzbget/license-unrar.txt"; then
			if [ "$SHORT" = "x86_64" ] && [ -f "$UNPACK_BASE/lib/x86_64-bsd/unrar/unrar" ]; then
				echo "Notice: using fallback unrar from unpack bundle for $SHORT"
				cp "$UNPACK_BASE/lib/x86_64-bsd/unrar/unrar" "$STAGING_DIR/nzbget/unrar-$SHORT"
				cp "$UNPACK_BASE/lib/x86_64-bsd/unrar/license-unrar.txt" "$STAGING_DIR/nzbget/license-unrar.txt"
			else
				echo "ERROR: unrar binary is required for FreeBSD $SHORT ($ARCH_TAG) but could not be obtained"
				exit 1
			fi
		fi

		if ! fetch_unpacker 7zip "$ZIP7_VERSION" "$ARCH_TAG" "7za:$STAGING_DIR/nzbget/7za-$SHORT" "license.txt:$STAGING_DIR/nzbget/license-7zip.txt"; then
			if [ "$SHORT" = "x86_64" ] && [ -f "$UNPACK_BASE/lib/x86_64-bsd/7zip/7za" ]; then
				echo "Notice: using fallback 7za from unpack bundle for $SHORT"
				cp "$UNPACK_BASE/lib/x86_64-bsd/7zip/7za" "$STAGING_DIR/nzbget/7za-$SHORT"
				cp "$UNPACK_BASE/lib/x86_64-bsd/7zip/license-7zip.txt" "$STAGING_DIR/nzbget/license-7zip.txt"
			else
				echo "ERROR: 7za binary is required for FreeBSD $SHORT ($ARCH_TAG) but could not be obtained"
				exit 1
			fi
		fi

		if [ ! -f "$STAGING_DIR/nzbget/unrar-$SHORT" ]; then
			echo "ERROR: unrar binary is missing in staging for FreeBSD $SHORT"
			exit 1
		fi
		if [ ! -f "$STAGING_DIR/nzbget/7za-$SHORT" ]; then
			echo "ERROR: 7za binary is missing in staging for FreeBSD $SHORT"
			exit 1
		fi

		chmod +x "$STAGING_DIR/nzbget/nzbget-$SHORT" "$STAGING_DIR/nzbget/unrar-$SHORT" "$STAGING_DIR/nzbget/7za-$SHORT"

		DISTARCHS=$(echo "$DISTARCHS $SHORT" | xargs)
	done

	local NZBGET_DIR="$STAGING_DIR/nzbget"
	local CONF="$NZBGET_DIR/webui/nzbget.conf.template"
	sed_i 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' "$CONF"
	sed_i 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' "$CONF"
	sed_i 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' "$CONF"
	sed_i 's:^CertCheck=.*:CertCheck=yes:' "$CONF"

	cp "$NZBGET_ROOT/platforms/freebsd/package-info.json" "$NZBGET_DIR/webui/package-info.json"
	cp "$NZBGET_ROOT/platforms/freebsd/install-update.sh" "$NZBGET_DIR/install-update.sh"
	cp "$NZBGET_ROOT/pubkey.pem" "$NZBGET_DIR/pubkey.pem"
	download_cacert "$SOURCE_DIR/cacert.pem"
	cp "$SOURCE_DIR/cacert.pem" "$NZBGET_DIR/cacert.pem"

	local INSTFILE="$DIST_BASE/$BASENAME-bin-$PLATFORM$SUFFIX.run"
	local DATA="$INSTFILE.data"
	(cd "$NZBGET_DIR" && tar -cf - * | gzip -c) > "$DATA"

	cp "$NZBGET_ROOT/platforms/freebsd/installer.sh" "$INSTFILE"
	sed_i "s:^TITLE=$:TITLE=\"$BASENAME$SUFFIX\":" "$INSTFILE"
	sed_i "s:^PLATFORM=$:PLATFORM=\"$PLATFORM\":" "$INSTFILE"
	sed_i "s:^DISTARCHS=$:DISTARCHS=\"$DISTARCHS\":" "$INSTFILE"
	sed_i "s:^MD5=$:MD5=\"$(md5_of "$DATA")\":" "$INSTFILE"

	local PAYLOAD PAYLOADLEN HEADER HEADERLEN TOTAL TOTALLEN
	PAYLOAD=$(size_of "$DATA")
	PAYLOADLEN=${#PAYLOAD}
	HEADER=$(size_of "$INSTFILE")
	HEADERLEN=${#HEADER}
	HEADER=$((HEADER + HEADERLEN + PAYLOADLEN))
	TOTAL=$((HEADER + PAYLOAD))
	TOTALLEN=${#TOTAL}
	HEADER=$((HEADER - PAYLOADLEN + TOTALLEN))
	TOTAL=$((TOTAL - PAYLOADLEN + TOTALLEN))

	sed_i "s:^HEADER=$:HEADER=$HEADER:" "$INSTFILE"
	sed_i "s:^TOTAL=$:TOTAL=$TOTAL:" "$INSTFILE"

	cat "$DATA" >> "$INSTFILE"
	rm -f "$DATA"
	chmod +x "$INSTFILE"

	echo "Installer created: $INSTFILE"
}

if [ "$TARGETS" = "bin" ] || [ "$TARGETS" = "all" ]; then
	for ARCH in $ARCHS; do
		build_arch "$ARCH"
		package_bin "$ARCH"
	done
fi

if [ "$TARGETS" = "installer" ] || [ "$TARGETS" = "all" ]; then
	package_installer
fi

echo "All FreeBSD targets completed successfully."
