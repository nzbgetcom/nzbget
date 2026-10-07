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

NZBGET_ROOT=$(cd "$(dirname "$0")/../.." && pwd)

# Unpackers versions (nzbgetcom/7zip and nzbgetcom/unrar release tags)
. "$NZBGET_ROOT/unpackers.env"

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Directories (flat layout under build/)
SOURCE_DIR="$NZBGET_ROOT/build/dl"
STAGING_BASE="$NZBGET_ROOT/build/staging"
DIST_BASE="$NZBGET_ROOT/build/dist"

mkdir -p "$SOURCE_DIR" "$STAGING_BASE" "$DIST_BASE"

# Parse command-line parameters
ALL_ARCHS="x64 arm64"
ARCHS=""
CONFIGS=""
TARGETS=""
UNIVERSAL="no"
TESTING="no"

for PARAM in "$@"; do
	case $PARAM in
		x64|x86_64)
			ARCHS=$(echo "$ARCHS x64" | xargs)
			;;
		arm64)
			ARCHS=$(echo "$ARCHS arm64" | xargs)
			;;
		universal)
			UNIVERSAL="yes"
			;;
		release|release-lto)
			CONFIGS=$(echo "$CONFIGS release" | xargs)
			;;
		debug)
			CONFIGS=$(echo "$CONFIGS debug" | xargs)
			;;
		testing)
			TESTING="yes"
			;;
		bin|app|package|all)
			TARGETS=$(echo "$TARGETS $PARAM" | xargs)
			;;
		*)
			echo "Invalid parameter: $PARAM"
			echo "Usage: bash platforms/macos/build.sh [bin|app|package|all] [x64|arm64|universal] [release|debug] [testing]"
			exit 1
			;;
	esac
done

if [ -z "$ARCHS" ]; then
	ARCHS="$ALL_ARCHS"
fi

if [ -z "$CONFIGS" ]; then
	CONFIGS="release"
fi

if [ -z "$TARGETS" ]; then
	TARGETS="all"
fi

VERSION=$(grep "set(VERSION " "$NZBGET_ROOT/CMakeLists.txt" | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ "$TESTING" == "yes" ]; then
	VERSION_SUFFIX="-testing-$(date '+%Y%m%d')"
fi

# Check if full Xcode.app (xcodebuild) is available
has_xcodebuild() {
	if command -v xcodebuild >/dev/null 2>&1 && xcodebuild -version >/dev/null 2>&1; then
		return 0
	fi
	return 1
}

# Download a release asset once (failures are fatal)
download() {
	local URL=$1
	local DEST=$2
	if [ ! -s "$DEST" ]; then
		echo "Downloading $URL"
		curl -fsSL --retry 3 -o "$DEST.part" "$URL"
		mv "$DEST.part" "$DEST"
	fi
}

# Helper: Fetch unpackers (7zip, unrar) and root certificates
fetch_unpackers() {
	local ARCH=$1
	local DEST_BIN=$2
	mkdir -p "$DEST_BIN" "$SOURCE_DIR"

	# 1. 7zip (universal binary from nzbgetcom/7zip)
	local TAR_7Z="$SOURCE_DIR/7zip-macos-universal-v${ZIP7_VERSION}.tar.gz"
	download "https://github.com/nzbgetcom/7zip/releases/download/v$ZIP7_VERSION/7zip-macos-universal.tar.gz" "$TAR_7Z"
	rm -rf "$SOURCE_DIR/7z_tmp"
	mkdir -p "$SOURCE_DIR/7z_tmp"
	tar -xzf "$TAR_7Z" -C "$SOURCE_DIR/7z_tmp"
	cp "$SOURCE_DIR/7z_tmp/7za" "$DEST_BIN/7za"
	rm -rf "$SOURCE_DIR/7z_tmp"

	# 2. unrar (arch-specific binary from nzbgetcom/unrar)
	local TAR_UNRAR="$SOURCE_DIR/unrar-macos-${ARCH}-v${UNRAR_VERSION}.tar.gz"
	download "https://github.com/nzbgetcom/unrar/releases/download/v$UNRAR_VERSION/unrar-macos-$ARCH.tar.gz" "$TAR_UNRAR"
	rm -rf "$SOURCE_DIR/unrar_tmp"
	mkdir -p "$SOURCE_DIR/unrar_tmp"
	tar -xzf "$TAR_UNRAR" -C "$SOURCE_DIR/unrar_tmp"
	cp "$SOURCE_DIR/unrar_tmp/unrar" "$DEST_BIN/unrar"
	rm -rf "$SOURCE_DIR/unrar_tmp"

	# 3. root certificates
	echo "Downloading latest root certificates from curl.se..."
	curl -fsSL --retry 3 -o "$DEST_BIN/cacert.pem" "https://curl.se/ca/cacert.pem"
}

# Adjust nzbget.conf paths for macOS
adjust_conf() {
	local CONF_FILE=$1
	sed -i '' 's:^MainDir=.*:MainDir=~/Library/Application Support/NZBGet:' "$CONF_FILE"
	sed -i '' 's:^DestDir=.*:DestDir=~/Downloads/NZBGet/complete:' "$CONF_FILE"
	sed -i '' 's:^InterDir=.*:InterDir=~/Downloads/NZBGet/intermediate:' "$CONF_FILE"
	sed -i '' 's:^NzbDir=.*:NzbDir=~/Downloads/NZBGet/nzb:' "$CONF_FILE"
	sed -i '' 's:^WebDir=.*:# NOTE\: option WebDir cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
	sed -i '' 's:^LockFile=.*:# NOTE\: option LockFile cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
	sed -i '' 's:^LogFile=.*:LogFile=~/Library/Logs/NZBGet.log:' "$CONF_FILE"
	sed -i '' 's:^OutputMode=.*:OutputMode=loggable:' "$CONF_FILE"
	sed -i '' '/# example configuration file (installed to/{N;s/.*/# example configuration file (installed to\n# \/usr\/local\/share\/nzbget\/nzbget.conf)./;}' "$CONF_FILE"
	sed -i '' 's:^ConfigTemplate=.*:# NOTE\: option ConfigTemplate cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
	sed -i '' 's:^DaemonUsername=.*:# NOTE\: option DaemonUsername cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
	sed -i '' 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' "$CONF_FILE"
	sed -i '' 's:^CertCheck=.*:CertCheck=yes:' "$CONF_FILE"
	sed -i '' 's:^AuthorizedIP=.*:AuthorizedIP=127.0.0.1:' "$CONF_FILE"
	sed -i '' 's:^ArticleCache=.*:ArticleCache=700:' "$CONF_FILE"
	sed -i '' 's:^DirectWrite=.*:DirectWrite=no:' "$CONF_FILE"
	sed -i '' 's:^WriteBuffer=.*:WriteBuffer=1024:' "$CONF_FILE"
	sed -i '' 's:^ParBuffer=.*:ParBuffer=500:' "$CONF_FILE"
	sed -i '' 's:^DirectRename=.*:DirectRename=yes:' "$CONF_FILE"
	sed -i '' 's:^DirectUnpack=.*:DirectUnpack=yes:' "$CONF_FILE"
	sed -i '' 's:^UnrarCmd=.*:UnrarCmd=${AppDir}/unrar:' "$CONF_FILE"
	sed -i '' 's:^SevenZipCmd=.*:SevenZipCmd=${AppDir}/7za:' "$CONF_FILE"
}

# Main build loop
for CONFIG in $CONFIGS; do
	for ARCH in $ARCHS; do
		TARGET_NAME="macos-$ARCH-$CONFIG"
		STAGING_DIR="$STAGING_BASE/$TARGET_NAME"
		
		# Determine CMake preset name and build type
		PRESET_NAME="ci-macos-$ARCH"
		if [ "$CONFIG" == "debug" ]; then
			CMAKE_BUILD_TYPE="Debug"
			SUFFIX="-debug"
		else
			CMAKE_BUILD_TYPE="Release"
			SUFFIX=""
		fi

		echo "=========================================================="
		echo "Building $TARGET_NAME (preset: $PRESET_NAME)..."
		echo "=========================================================="

		rm -rf "$STAGING_DIR"
		mkdir -p "$STAGING_DIR"

		# 1. CMake configure & build via preset
		cd "$NZBGET_ROOT"
		
		# Configure
		cmake --preset "$PRESET_NAME" \
			-DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
			-DVERSION_SUFFIX="$VERSION_SUFFIX" \
			-DCMAKE_INSTALL_PREFIX="$STAGING_DIR" \
			-DCMAKE_OSX_DEPLOYMENT_TARGET="12.0"

		# Build
		BUILD_STATUS=""
		cmake --build --preset "$PRESET_NAME" -j "$JOBS" 2>build.log || BUILD_STATUS=$?
		if [ -n "$BUILD_STATUS" ]; then
			tail -30 build.log
			exit 1
		fi

		# 2. Strip and install (bin/nzbget, share/nzbget/{webui,doc,nzbget.conf})
		BUILD_DIR="$NZBGET_ROOT/build/$PRESET_NAME"
		DAEMON_BIN="$BUILD_DIR/nzbget"
		if [ ! -f "$DAEMON_BIN" ]; then
			echo "ERROR: nzbget binary not found at $DAEMON_BIN"
			exit 1
		fi

		if [ "$CONFIG" != "debug" ]; then
			strip "$DAEMON_BIN"
		fi
		cmake --install "$BUILD_DIR" >/dev/null

		adjust_conf "$STAGING_DIR/share/nzbget/nzbget.conf"

		# 3. Fetch unpackers into dist/bin
		fetch_unpackers "$ARCH" "$STAGING_DIR/bin"

		echo "Artifacts ready in: $STAGING_DIR/bin"
		ls -la "$STAGING_DIR/bin"

		# 4. Packaging (App / Zip)
		BUILD_APP="no"
		BUILD_BIN="no"

		if [[ "$TARGETS" == *"bin"* ]]; then
			BUILD_BIN="yes"
		fi

		if [[ "$TARGETS" == *"app"* ]]; then
			BUILD_APP="yes"
		fi

		if [[ "$TARGETS" == *"all"* ]] || [[ "$TARGETS" == *"package"* ]]; then
			if has_xcodebuild; then
				BUILD_APP="yes"
			else
				BUILD_BIN="yes"
			fi
		fi

		APP_ARCHIVE_NAME="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-$ARCH$SUFFIX.zip"
		if [ "$BUILD_APP" == "yes" ]; then
			BIN_ARCHIVE_NAME="nzbget-$VERSION$VERSION_SUFFIX-macos-$ARCH-bin$SUFFIX.zip"
		else
			BIN_ARCHIVE_NAME="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-$ARCH$SUFFIX.zip"
		fi

		if [ "$BUILD_BIN" == "yes" ]; then
			echo "Packaging binary distribution ($TARGET_NAME)..."
			(cd "$STAGING_DIR" && zip -r "$BIN_ARCHIVE_NAME" bin share >/dev/null)
			mv "$STAGING_DIR/$BIN_ARCHIVE_NAME" "$DIST_BASE/$BIN_ARCHIVE_NAME"
			echo "Binary distribution archive created: $DIST_BASE/$BIN_ARCHIVE_NAME"
		fi

		if [ "$BUILD_APP" == "yes" ]; then
			if has_xcodebuild; then
				echo "Building macOS GUI App ($TARGET_NAME)..."
				# The xcodeproj expects daemon payload at Resources/daemon/usr/local
				# Isolate Xcode build in staging directory to avoid polluting tracked sources
				XCODE_STAGE="$STAGING_DIR/xcode"
				rm -rf "$XCODE_STAGE"
				mkdir -p "$XCODE_STAGE"
				cp -R "$NZBGET_ROOT/platforms/macos/"* "$XCODE_STAGE/"
				APP_DAEMON_ROOT="$XCODE_STAGE/Resources/daemon/usr/local"
				rm -rf "$APP_DAEMON_ROOT"
				mkdir -p "$APP_DAEMON_ROOT/bin" "$APP_DAEMON_ROOT/share"
				cp "$STAGING_DIR/bin/nzbget" "$STAGING_DIR/bin/7za" "$STAGING_DIR/bin/unrar" "$STAGING_DIR/bin/cacert.pem" "$APP_DAEMON_ROOT/bin/"
				# Copy share (webui, doc, nzbget.conf)
				cp -r "$STAGING_DIR/share/nzbget" "$APP_DAEMON_ROOT/share/"

				cd "$XCODE_STAGE"
				xcodebuild -project "$XCODE_STAGE/NZBGet.xcodeproj" -configuration "Release" -destination "platform=macOS" SYMROOT="$XCODE_STAGE/build" build >xcodebuild.log 2>&1 || {
					tail -30 xcodebuild.log
					exit 1
				}
				# Find the built app
				APP_PATH=$(find "$XCODE_STAGE/build" -name "NZBGet.app" -type d 2>/dev/null | head -1)
				if [ -n "$APP_PATH" ]; then
					(cd "$(dirname "$APP_PATH")" && zip -r "$APP_ARCHIVE_NAME" "$(basename "$APP_PATH")" >/dev/null)
					mv "$(dirname "$APP_PATH")/$APP_ARCHIVE_NAME" "$DIST_BASE/$APP_ARCHIVE_NAME"
					echo "App bundle archive created: $DIST_BASE/$APP_ARCHIVE_NAME"
				else
					echo "ERROR: NZBGet.app not found after xcodebuild"
					exit 1
				fi
			else
				echo "ERROR: Full Xcode.app (xcodebuild) is required to build macOS GUI App"
				exit 1
			fi
		fi

		cd "$NZBGET_ROOT"
	done

	# 5. Universal binary & package
	if [ "$UNIVERSAL" == "yes" ]; then
		UNI_NAME="macos-universal-$CONFIG"
		UNI_DIR="$STAGING_BASE/$UNI_NAME"
		echo "=========================================================="
		echo "Creating Universal package ($UNI_NAME)..."
		echo "=========================================================="
		mkdir -p "$UNI_DIR/bin" "$UNI_DIR/share"
		
		ARM64_DIR="$STAGING_BASE/macos-arm64-$CONFIG"
		X64_DIR="$STAGING_BASE/macos-x64-$CONFIG"
		
		cp -r "$ARM64_DIR/share/nzbget" "$UNI_DIR/share/"
		
		lipo -create "$X64_DIR/bin/nzbget" "$ARM64_DIR/bin/nzbget" -output "$UNI_DIR/bin/nzbget"
		lipo -create "$X64_DIR/bin/unrar" "$ARM64_DIR/bin/unrar" -output "$UNI_DIR/bin/unrar"
		cp "$ARM64_DIR/bin/7za" "$UNI_DIR/bin/7za"
		cp "$ARM64_DIR/bin/cacert.pem" "$UNI_DIR/bin/cacert.pem"

		echo "Universal artifacts created in: $UNI_DIR/bin"
		lipo -info "$UNI_DIR/bin/nzbget"
		lipo -info "$UNI_DIR/bin/unrar"

		BUILD_UNI_APP="no"
		BUILD_UNI_BIN="no"

		if [[ "$TARGETS" == *"bin"* ]]; then
			BUILD_UNI_BIN="yes"
		fi

		if [[ "$TARGETS" == *"app"* ]]; then
			BUILD_UNI_APP="yes"
		fi

		if [[ "$TARGETS" == *"all"* ]] || [[ "$TARGETS" == *"package"* ]]; then
			if has_xcodebuild && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64$SUFFIX.zip" ] && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-arm64$SUFFIX.zip" ]; then
				BUILD_UNI_APP="yes"
			else
				BUILD_UNI_BIN="yes"
			fi
		fi

		UNI_APP_ARCHIVE="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-universal$SUFFIX.zip"
		if [ "$BUILD_UNI_APP" == "yes" ]; then
			UNI_BIN_ARCHIVE="nzbget-$VERSION$VERSION_SUFFIX-macos-universal-bin$SUFFIX.zip"
		else
			UNI_BIN_ARCHIVE="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-universal$SUFFIX.zip"
		fi

		if [ "$BUILD_UNI_BIN" == "yes" ]; then
			echo "Packaging Universal binary distribution archive..."
			(cd "$UNI_DIR" && zip -r "$UNI_BIN_ARCHIVE" bin share >/dev/null)
			mv "$UNI_DIR/$UNI_BIN_ARCHIVE" "$DIST_BASE/$UNI_BIN_ARCHIVE"
			echo "Universal binary archive created: $DIST_BASE/$UNI_BIN_ARCHIVE"
		fi

		if [ "$BUILD_UNI_APP" == "yes" ]; then
			if has_xcodebuild && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64$SUFFIX.zip" ] && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-arm64$SUFFIX.zip" ]; then
				UNI_TMP="$NZBGET_ROOT/build/universal-app-tmp"
				rm -rf "$UNI_TMP"
				mkdir -p "$UNI_TMP"
				cd "$UNI_TMP"
				unzip -qo "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64$SUFFIX.zip"
				if [ ! -d "NZBGet.app" ]; then
					echo "ERROR: NZBGet.app not found in x64 package"
					exit 1
				fi
				mv NZBGet.app NZBGet.x64.app
				unzip -qo "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-arm64$SUFFIX.zip"
				if [ ! -d "NZBGet.app" ]; then
					echo "ERROR: NZBGet.app not found in arm64 package"
					exit 1
				fi
				mv NZBGet.app NZBGet.arm64.app

				DAEMON_REL="Contents/Resources/daemon/usr/local/bin"
				lipo -create "NZBGet.x64.app/$DAEMON_REL/nzbget" "NZBGet.arm64.app/$DAEMON_REL/nzbget" -output "$UNI_TMP/nzbget"
				lipo -create "NZBGet.x64.app/$DAEMON_REL/unrar" "NZBGet.arm64.app/$DAEMON_REL/unrar" -output "$UNI_TMP/unrar"
				
				mv NZBGet.arm64.app NZBGet.app
				mv "$UNI_TMP/nzbget" "NZBGet.app/$DAEMON_REL/nzbget"
				mv "$UNI_TMP/unrar" "NZBGet.app/$DAEMON_REL/unrar"

				zip -r "$UNI_APP_ARCHIVE" NZBGet.app >/dev/null
				mv "$UNI_APP_ARCHIVE" "$DIST_BASE/$UNI_APP_ARCHIVE"
				rm -rf "$UNI_TMP"
				echo "Universal app archive created: $DIST_BASE/$UNI_APP_ARCHIVE"
			else
				echo "ERROR: Full Xcode.app and per-architecture app packages are required for universal app"
				exit 1
			fi
		fi
		cd "$NZBGET_ROOT"
	fi
done

echo "Done."
echo "Artifacts are located in: $DIST_BASE"
