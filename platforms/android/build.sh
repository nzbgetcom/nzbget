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
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

set -o nounset
set -o errexit
set -o pipefail

NZBGET_ROOT=$(cd "$(dirname "$0")/../.." && pwd)

# Unpackers versions (nzbgetcom/7zip and nzbgetcom/unrar release tags)
if [ -f "$NZBGET_ROOT/unpackers.env" ]; then
	. "$NZBGET_ROOT/unpackers.env"
fi
UNRAR6_VERSION="${UNRAR6_VERSION:-6.24}"
UNRAR7_VERSION="${UNRAR7_VERSION:-${UNRAR_VERSION:-7.23}}"
ZIP7_VERSION="${ZIP7_VERSION:-26.03}"

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

SOURCE_DIR="$NZBGET_ROOT/build/dl"
STAGING_BASE="$NZBGET_ROOT/build/staging"
DIST_BASE="$NZBGET_ROOT/build/dist"

mkdir -p "$SOURCE_DIR" "$STAGING_BASE" "$DIST_BASE"

ALL_ARCHS="arm64 armv7 x86_64 x86"
ARCHS=""
TARGETS=""
TESTING="no"
CONFIG="release"
SUFFIX=""
PRESET_PREFIX="ci-android"
API_LEVEL="${ANDROID_API_LEVEL:-21}"

USAGE="Usage: bash platforms/android/build.sh [bin|installer|all] [arm64|armv7|x86_64|x86|all-arch] [release|debug] [testing]"

for PARAM in "$@"; do
	case $PARAM in
		arm64|aarch64)
			ARCHS=$(echo "$ARCHS arm64" | xargs)
			;;
		armv7|armeabi-v7a|armhf)
			ARCHS=$(echo "$ARCHS armv7" | xargs)
			;;
		x86_64|x64)
			ARCHS=$(echo "$ARCHS x86_64" | xargs)
			;;
		x86|i686)
			ARCHS=$(echo "$ARCHS x86" | xargs)
			;;
		all-arch)
			ARCHS="$ALL_ARCHS"
			;;
		release)
			;;
		debug)
			CONFIG="debug"
			SUFFIX="-debug"
			PRESET_PREFIX="ci-android-debug"
			;;
		testing)
			TESTING="yes"
			;;
		bin|installer|all)
			TARGETS=$(echo "$TARGETS $PARAM" | xargs)
			;;
		*)
			echo "Invalid parameter: $PARAM"
			echo "$USAGE"
			exit 1
			;;
	esac
done

if [ -z "$ARCHS" ]; then
	ARCHS="arm64"
fi

if [ -z "$TARGETS" ]; then
	TARGETS="all"
fi

VERSION=$(grep "set(VERSION " "$NZBGET_ROOT/CMakeLists.txt" | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ "$TESTING" = "yes" ]; then
	VERSION_SUFFIX="-testing-${BUILD_DATE:-$(date '+%Y%m%d')}"
fi
BASENAME="nzbget-${VERSION}${VERSION_SUFFIX}"
PLATFORM="android"

echo "=== NZBGet Android Builder ==="
echo "Version:       $VERSION$VERSION_SUFFIX"
echo "Targets:       $TARGETS"
echo "Architectures: $ARCHS"
echo "Config:        $CONFIG"

# Short architecture name used in package file names (same as the other platforms)
short_arch() {
	case "$1" in
		arm64) echo "aarch64" ;;
		armv7) echo "armhf" ;;
		x86_64) echo "x86_64" ;;
		x86) echo "i686" ;;
	esac
}

# Portable in-place sed (GNU and BSD)
sed_i() {
	sed "$1" "$2" > "$2.tmp"
	mv "$2.tmp" "$2"
}

md5_of() {
	if command -v md5sum >/dev/null 2>&1; then
		md5sum "$1" | cut -b-32
	else
		md5 -q "$1"
	fi
}

size_of() {
	wc -c < "$1" | tr -d ' '
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

# Ensure LLVM 19.1.7, Bionic sysroot, compiler-rt and static libc++ are available
ensure_toolchain() {
	local ARCH=$1
	ANDROID_API_LEVEL="$API_LEVEL" bash "$NZBGET_ROOT/toolchains/setup-android-toolchain.sh" "$ARCH"
}

# Download a nzbgetcom unpacker release asset and copy the requested files
# usage: fetch_unpacker <repo> <version> <short arch> <src name>:<dest> ...
fetch_unpacker() {
	local REPO=$1 VER=$2 SHORT=$3
	shift 3
	local TAR="$SOURCE_DIR/$REPO-$SHORT-ndk-v$VER.tar.gz"
	local TMP="$SOURCE_DIR/$REPO-$SHORT-ndk-tmp"

	download "https://github.com/nzbgetcom/$REPO/releases/download/v$VER/$REPO-$SHORT-ndk.tar.gz" "$TAR"
	rm -rf "$TMP"
	mkdir -p "$TMP"
	tar -xzf "$TAR" -C "$TMP"
	local ITEM
	for ITEM in "$@"; do
		if [ ! -f "$TMP/${ITEM%%:*}" ]; then
			echo "ERROR: ${ITEM%%:*} not found in $TAR"
			exit 1
		fi
		cp "$TMP/${ITEM%%:*}" "${ITEM#*:}"
	done
	rm -rf "$TMP"
}

# Build binary for architecture
build_arch() {
	local ARCH=$1
	local PRESET="${PRESET_PREFIX}-${ARCH}"
	local BUILD_DIR="$NZBGET_ROOT/build/$PRESET"

	echo "--- Building NZBGet for Android ($ARCH) using preset: $PRESET (API $API_LEVEL) ---"
	ensure_toolchain "$ARCH"

	mkdir -p "$BUILD_DIR"
	cmake --preset "$PRESET" -DVERSION_SUFFIX="$VERSION_SUFFIX" -DANDROID_API_LEVEL="$API_LEVEL"
	cmake --build --preset "$PRESET" -j "$JOBS" 2>&1 | tee "$BUILD_DIR/build.log"

	local BIN_PATH="$BUILD_DIR/nzbget"
	if [ ! -f "$BIN_PATH" ]; then
		echo "ERROR: Expected output binary not found: $BIN_PATH"
		exit 1
	fi
	echo "Successfully built: $BIN_PATH"
}

# Per-architecture binary package (nzbget/ with binary, webui, docs, config template)
package_bin() {
	local ARCH=$1
	local SHORT
	SHORT=$(short_arch "$ARCH")
	local BUILD_DIR="$NZBGET_ROOT/build/${PRESET_PREFIX}-${ARCH}"
	local STAGING_DIR="$STAGING_BASE/bin-$ARCH"
	local TAR_PKG="$DIST_BASE/$BASENAME-bin-$PLATFORM-$SHORT$SUFFIX.tar.gz"

	echo "--- Packaging binary for Android ($ARCH) ---"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR/nzbget"

	cp "$BUILD_DIR/nzbget" "$STAGING_DIR/nzbget/nzbget"
	cp -R "$NZBGET_ROOT/webui" "$STAGING_DIR/nzbget/webui"
	cp "$NZBGET_ROOT/ChangeLog.md" "$NZBGET_ROOT/COPYING" "$STAGING_DIR/nzbget"

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

	rm -f "$TAR_PKG"
	tar -czf "$TAR_PKG" -C "$STAGING_DIR" nzbget
	rm -rf "$STAGING_DIR"
	echo "Created binary package: $TAR_PKG"
}

# Multi-architecture self-extracting installer built from per-arch binary packages
package_installer() {
	local STAGING_DIR="$STAGING_BASE/installer"
	local INSTFILE="$DIST_BASE/$BASENAME-bin-$PLATFORM$SUFFIX.run"
	local ARCH SHORT DISTARCHS=""

	echo "--- Creating Android installer for: $ARCHS ---"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR"

	for ARCH in $ARCHS; do
		SHORT=$(short_arch "$ARCH")
		local TAR_PKG="$DIST_BASE/$BASENAME-bin-$PLATFORM-$SHORT$SUFFIX.tar.gz"
		if [ ! -f "$TAR_PKG" ]; then
			# Fallback: search for matching binary package if date suffix differed
			local CANDIDATE
			CANDIDATE=$(ls -1 "$DIST_BASE"/nzbget-${VERSION}-*-bin-${PLATFORM}-${SHORT}${SUFFIX}.tar.gz 2>/dev/null | head -n 1 || true)
			if [ -n "$CANDIDATE" ] && [ -f "$CANDIDATE" ]; then
				TAR_PKG="$CANDIDATE"
			else
				echo "ERROR: Could not find $TAR_PKG"
				exit 1
			fi
		fi
		tar -xzf "$TAR_PKG" -C "$STAGING_DIR"
		mv "$STAGING_DIR/nzbget/nzbget" "$STAGING_DIR/nzbget/nzbget-$SHORT"
		fetch_unpacker unrar "$UNRAR6_VERSION" "$SHORT" "unrar:$STAGING_DIR/nzbget/unrar-$SHORT" "license-unrar.txt:$STAGING_DIR/nzbget/license-unrar.txt"
		fetch_unpacker unrar "$UNRAR7_VERSION" "$SHORT" "unrar:$STAGING_DIR/nzbget/unrar7-$SHORT"
		fetch_unpacker 7zip "$ZIP7_VERSION" "$SHORT" "7za:$STAGING_DIR/nzbget/7za-$SHORT" "license-7zip.txt:$STAGING_DIR/nzbget/license-7zip.txt"
		chmod +x "$STAGING_DIR/nzbget/unrar-$SHORT" "$STAGING_DIR/nzbget/unrar7-$SHORT" "$STAGING_DIR/nzbget/7za-$SHORT"
		DISTARCHS=$(echo "$DISTARCHS $SHORT" | xargs)
	done

	local NZBGET_DIR="$STAGING_DIR/nzbget"
	local CONF="$NZBGET_DIR/webui/nzbget.conf.template"
	sed_i 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' "$CONF"
	sed_i 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' "$CONF"
	sed_i 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' "$CONF"
	sed_i 's:^CertCheck=.*:CertCheck=yes:' "$CONF"

	cp "$NZBGET_ROOT/platforms/android/package-info.json" "$NZBGET_DIR/webui/package-info.json"
	cp "$NZBGET_ROOT/platforms/android/install-update.sh" "$NZBGET_DIR/install-update.sh"
	cp "$NZBGET_ROOT/pubkey.pem" "$NZBGET_DIR/pubkey.pem"
	download_cacert "$SOURCE_DIR/cacert.pem"
	cp "$SOURCE_DIR/cacert.pem" "$NZBGET_DIR/cacert.pem"

	# Payload
	local DATA="$INSTFILE.data"
	(cd "$NZBGET_DIR" && tar -cf - * | gzip -c) > "$DATA"

	# Installer header
	cp "$NZBGET_ROOT/platforms/android/installer.sh" "$INSTFILE"
	sed_i "s:^TITLE=$:TITLE=\"$BASENAME$SUFFIX\":" "$INSTFILE"
	sed_i "s:^PLATFORM=$:PLATFORM=\"$PLATFORM\":" "$INSTFILE"
	sed_i "s:^DISTARCHS=$:DISTARCHS=\"$DISTARCHS\":" "$INSTFILE"
	sed_i "s:^MD5=$:MD5=\"$(md5_of "$DATA")\":" "$INSTFILE"

	# HEADER and TOTAL contain their own digit counts
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
	rm -rf "$STAGING_DIR"
	echo "Created installer: $INSTFILE"
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

echo "=== Android Build & Packaging Finished Successfully ==="
