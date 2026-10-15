#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
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

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

# Target action: bin | installer | all
TARGET="${1:-all}"
# Architecture: x86_64 | aarch64 | armhf | armel | i686 | ppc6xx | riscv64 | all
ARCH_ARG="${2:-all}"
# Configuration: release | debug
CONFIG="${3:-release}"
# Testing flag: testing | ""
TESTING_ARG="${4:-}"

ALL_ARCHS="x86_64 aarch64 armhf armel i686 ppc6xx riscv64"
if [ "$ARCH_ARG" = "all" ] || [ "$ARCH_ARG" = "all-arch" ]; then
	ARCHS="$ALL_ARCHS"
else
	ARCHS="$ARCH_ARG"
fi

PLATFORM="linux"

SOURCE_DIR="$NZBGET_ROOT/build/dl"
OUTPUT_DIR="$NZBGET_ROOT/build/dist"
STAGING_BASE="$NZBGET_ROOT/build/staging"

# Version definitions from unpackers.env
if [ -f "$NZBGET_ROOT/unpackers.env" ]; then
	# shellcheck disable=SC1091
	. "$NZBGET_ROOT/unpackers.env"
else
	ZIP7_VERSION=26.03
	UNRAR_VERSION=7.23
fi

# Detect version from CMakeLists.txt
VERSION=$(grep -m1 "set(VERSION " "$NZBGET_ROOT/CMakeLists.txt" | cut -d '"' -f 2)

TESTING="no"
if [ "$TESTING_ARG" = "testing" ] || [ "${TESTING:-no}" = "yes" ]; then
	TESTING="yes"
fi

VERSION_SUFFIX=""
if [ "$TESTING" = "yes" ]; then
	VERSION_SUFFIX="-testing-${BUILD_DATE:-$(date '+%Y%m%d')}"
fi

BASENAME="nzbget-${VERSION}${VERSION_SUFFIX}"
SUFFIX=""
PRESET_PREFIX="ci-linux"

if [ "$CONFIG" = "debug" ]; then
	SUFFIX="-debug"
	PRESET_PREFIX="ci-linux-debug"
fi

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
		mkdir -p "$(dirname "$DEST")"
		curl -fsSL --retry 3 -o "$DEST.part" "$URL"
		mv "$DEST.part" "$DEST"
	fi
}

download_cacert() {
	local dest=$1
	local sha_file="$dest.sha256"
	if [ ! -s "$dest" ] || [ ! -s "$sha_file" ]; then
		echo "Downloading https://curl.se/ca/cacert.pem and verifying checksum..."
		mkdir -p "$(dirname "$dest")"
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

fetch_unpacker() {
	local REPO=$1 VER=$2 ARCH_TAG=$3
	shift 3
	local TAR="$SOURCE_DIR/$REPO-$ARCH_TAG-v$VER.tar.gz"
	local TMP="$SOURCE_DIR/$REPO-$ARCH_TAG-tmp"
	local URL="https://github.com/nzbgetcom/$REPO/releases/download/v$VER/$REPO-$ARCH_TAG.tar.gz"

	if [ ! -s "$TAR" ]; then
		echo "Fetching unpacker: $URL"
		mkdir -p "$SOURCE_DIR"
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
			mkdir -p "$(dirname "${ITEM#*:}")"
			cp "$TMP/${ITEM%%:*}" "${ITEM#*:}"
			chmod +x "${ITEM#*:}"
		fi
	done
	rm -rf "$TMP"
	return 0
}

ensure_toolchain() {
	local ARCH=$1
	local TOOLCHAIN_DIR="${NZBGET_LINUX_TOOLCHAIN_DIR:-$NZBGET_ROOT/build/toolchains/linux}"
	if [ ! -d "$TOOLCHAIN_DIR/$ARCH/bin" ]; then
		echo "Musl toolchain for $ARCH missing. Running toolchains/linux/build-toolchain.sh..."
		bash "$NZBGET_ROOT/toolchains/linux/build-toolchain.sh" "$ARCH"
	fi
}

build_arch() {
	local ARCH=$1
	local PRESET="${PRESET_PREFIX}-${ARCH}"
	local BUILD_DIR="$NZBGET_ROOT/build/$PRESET"

	echo "--- Building binary for $ARCH ($CONFIG, preset: $PRESET) ---"
	ensure_toolchain "$ARCH"
	mkdir -p "$BUILD_DIR"

	cmake --preset "$PRESET" -DLINUX_ARCH="$ARCH" -DVERSION_SUFFIX="$VERSION_SUFFIX"
	cmake --build --preset "$PRESET" 2>&1 | tee "$BUILD_DIR/build.log"

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
	local PRESET="${PRESET_PREFIX}-${ARCH}"
	local BUILD_DIR="$NZBGET_ROOT/build/$PRESET"
	local STAGING_DIR="$STAGING_BASE/bin-$ARCH"
	local TAR_PKG="$OUTPUT_DIR/$BASENAME-bin-$PLATFORM-$ARCH$SUFFIX.tar.gz"

	echo "--- Packaging binary archive for $ARCH ($CONFIG) ---"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR/nzbget"

	cp "$BUILD_DIR/nzbget" "$STAGING_DIR/nzbget/nzbget"
	cp -r "$NZBGET_ROOT/webui" "$STAGING_DIR/nzbget/webui"
	cp "$NZBGET_ROOT/ChangeLog.md" "$STAGING_DIR/nzbget/ChangeLog.md"
	cp "$NZBGET_ROOT/ChangeLog.md" "$STAGING_DIR/nzbget/ChangeLog.txt"
	cp "$NZBGET_ROOT/COPYING" "$STAGING_DIR/nzbget/COPYING"
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
	sed_i 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' "$CONF"
	sed_i 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' "$CONF"
	sed_i 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' "$CONF"
	sed_i 's:^CertCheck=.*:CertCheck=yes:' "$CONF"

	# Fetch unpackers (unrar v7.23, 7za v26.03)
	if fetch_unpacker unrar "$UNRAR_VERSION" "$ARCH" "unrar:$STAGING_DIR/nzbget/unrar" "license-unrar.txt:$STAGING_DIR/nzbget/license-unrar.txt"; then
		chmod +x "$STAGING_DIR/nzbget/unrar"
	fi
	if fetch_unpacker 7zip "$ZIP7_VERSION" "$ARCH" "7za:$STAGING_DIR/nzbget/7za" "license-7zip.txt:$STAGING_DIR/nzbget/license-7zip.txt"; then
		chmod +x "$STAGING_DIR/nzbget/7za"
	fi

	download_cacert "$SOURCE_DIR/cacert.pem"
	cp "$SOURCE_DIR/cacert.pem" "$STAGING_DIR/nzbget/cacert.pem"

	mkdir -p "$OUTPUT_DIR"
	(cd "$STAGING_DIR" && tar -czf "$TAR_PKG" nzbget)
	echo "Created archive: $TAR_PKG"
}

package_installer() {
	echo "--- Packaging self-extracting .run installer ($CONFIG) ---"
	local STAGING_DIR="$STAGING_BASE/installer"
	rm -rf "$STAGING_DIR"
	mkdir -p "$STAGING_DIR/nzbget"

	local DISTARCHS=""
	for ARCH in $ARCHS; do
		local TAR_PKG="$OUTPUT_DIR/$BASENAME-bin-$PLATFORM-$ARCH$SUFFIX.tar.gz"
		if [ ! -f "$TAR_PKG" ]; then
			echo "ERROR: required package archive not found: $TAR_PKG"
			echo "Please build binary archives first with: $0 bin all $CONFIG"
			exit 1
		fi
		tar -xzf "$TAR_PKG" -C "$STAGING_DIR"
		mv "$STAGING_DIR/nzbget/nzbget" "$STAGING_DIR/nzbget/nzbget-$ARCH"

		if [ -f "$STAGING_DIR/nzbget/unrar" ]; then
			mv "$STAGING_DIR/nzbget/unrar" "$STAGING_DIR/nzbget/unrar-$ARCH"
		fi
		if [ -f "$STAGING_DIR/nzbget/7za" ]; then
			mv "$STAGING_DIR/nzbget/7za" "$STAGING_DIR/nzbget/7za-$ARCH"
		fi

		DISTARCHS=$(echo "$DISTARCHS $ARCH" | xargs)
	done

	local NZBGET_DIR="$STAGING_DIR/nzbget"
	local CONF="$NZBGET_DIR/webui/nzbget.conf.template"
	sed_i 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' "$CONF"
	sed_i 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' "$CONF"
	sed_i 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' "$CONF"
	sed_i 's:^CertCheck=.*:CertCheck=yes:' "$CONF"

	cp "$NZBGET_ROOT/platforms/linux/package-info.json" "$NZBGET_DIR/webui/package-info.json"
	cp "$NZBGET_ROOT/platforms/linux/install-update.sh" "$NZBGET_DIR/install-update.sh"
	cp "$NZBGET_ROOT/pubkey.pem" "$NZBGET_DIR/pubkey.pem"
	download_cacert "$SOURCE_DIR/cacert.pem"
	cp "$SOURCE_DIR/cacert.pem" "$NZBGET_DIR/cacert.pem"

	local INSTFILE="$OUTPUT_DIR/$BASENAME-bin-$PLATFORM$SUFFIX.run"
	local DATA="$INSTFILE.data"
	mkdir -p "$OUTPUT_DIR"
	(cd "$NZBGET_DIR" && tar -cf - * | gzip -c) > "$DATA"

	cp "$NZBGET_ROOT/platforms/linux/installer.sh" "$INSTFILE"
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

	echo "Self-extracting installer successfully created: $INSTFILE"
}

case "$TARGET" in
	build)
		for arch in $ARCHS; do
			build_arch "$arch"
		done
		;;
	bin)
		for arch in $ARCHS; do
			build_arch "$arch"
			package_bin "$arch"
		done
		;;
	installer)
		package_installer
		;;
	all)
		for arch in $ARCHS; do
			build_arch "$arch"
			package_bin "$arch"
		done
		package_installer
		;;
	*)
		echo "Usage: $0 [build|bin|installer|all] [arch|all] [release|debug] [testing]"
		exit 1
		;;
esac
