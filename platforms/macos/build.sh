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

# Unpackers versions defaults (can be overridden by environment variables)
UNRAR_VERSION="${UNRAR_VERSION-723}"
ZIP7_VERSION="${ZIP7_VERSION-26.02}"

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
NZBGET_ROOT=$(cd "$(dirname "$0")/../.." && pwd)

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

# Helper: Fetch or build unpackers (7zip, unrar, cacert)
fetch_unpackers() {
    local ARCH=$1
    local DEST_BIN=$2
    mkdir -p "$DEST_BIN" "$SOURCE_DIR"

    # 1. 7zip
    local TAR_7Z="$SOURCE_DIR/7z${ZIP7_VERSION//./}-mac.tar.xz"
    if [ ! -f "$TAR_7Z" ]; then
        local URL_7Z="https://github.com/ip7z/7zip/releases/download/$ZIP7_VERSION/7z${ZIP7_VERSION//./}-mac.tar.xz"
        curl -fsSL -o "$TAR_7Z" "$URL_7Z" || true
    fi
    if [ -f "$TAR_7Z" ]; then
        mkdir -p "$SOURCE_DIR/7z_tmp"
        tar -xf "$TAR_7Z" -C "$SOURCE_DIR/7z_tmp"
        cp "$SOURCE_DIR/7z_tmp/7zz" "$DEST_BIN/7za"
        rm -rf "$SOURCE_DIR/7z_tmp"
    fi

    # 2. unrar
    local UNRAR_ARCH=$ARCH
    [ "$ARCH" == "arm64" ] && UNRAR_ARCH="arm"
    [ "$ARCH" == "x64" ] && UNRAR_ARCH="x64"
    local TAR_UNRAR="$SOURCE_DIR/rarmacos-$UNRAR_ARCH-$UNRAR_VERSION.tar.gz"
    if [ ! -f "$TAR_UNRAR" ]; then
        local URL_UNRAR="https://www.rarlab.com/rar/rarmacos-$UNRAR_ARCH-$UNRAR_VERSION.tar.gz"
        curl -fsSL -o "$TAR_UNRAR" "$URL_UNRAR" || true
    fi
    if [ -f "$TAR_UNRAR" ]; then
        mkdir -p "$SOURCE_DIR/unrar_tmp"
        tar -xf "$TAR_UNRAR" -C "$SOURCE_DIR/unrar_tmp"
        cp "$SOURCE_DIR/unrar_tmp/rar/unrar" "$DEST_BIN/unrar"
        rm -rf "$SOURCE_DIR/unrar_tmp"
    fi

    # 3. root certificates
    if [ ! -f "$SOURCE_DIR/cacert.pem" ]; then
        curl -fsSL -o "$SOURCE_DIR/cacert.pem" https://curl.se/ca/cacert.pem || true
    fi
    [ -f "$SOURCE_DIR/cacert.pem" ] && cp "$SOURCE_DIR/cacert.pem" "$DEST_BIN/cacert.pem"
}

# Adjust nzbget.conf paths for macOS
adjust_conf() {
    local CONF_FILE=$1
    sed -i '' 's:^MainDir=.*:MainDir=~/Library/Application Support/NZBGet:' "$CONF_FILE"
    sed -i '' 's:^DestDir=.*:DestDir=~/Downloads:' "$CONF_FILE"
    sed -i '' 's:^InterDir=.*:InterDir=~/Downloads/Intermediate:' "$CONF_FILE"
    sed -i '' 's:^WebDir=.*:# NOTE\: option WebDir cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
    sed -i '' 's:^LockFile=.*:# NOTE\: option LockFile cannot be changed because it is hardcoded in OSX version.:' "$CONF_FILE"
    sed -i '' 's:^LogFile=.*:LogFile=~/Library/Logs/NZBGet.log:' "$CONF_FILE"
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
        
        # Determine CMake preset name
        if [ "$CONFIG" == "debug" ]; then
            PRESET_NAME="ci-macos-$ARCH"
            # For debug we'll need a separate debug preset or override
            CMAKE_BUILD_TYPE="Debug"
            SUFFIX="-debug"
        else
            PRESET_NAME="ci-macos-$ARCH"
            CMAKE_BUILD_TYPE="Release"
            SUFFIX=""
        fi

        echo "=========================================================="
        echo "Building $TARGET_NAME (preset: $PRESET_NAME)..."
        echo "=========================================================="

        rm -rf "$STAGING_DIR"
        mkdir -p "$STAGING_DIR/bin" "$STAGING_DIR/share/nzbget"

        # 1. CMake configure & build via preset
        cd "$NZBGET_ROOT"
        
        # Configure
        cmake --preset "$PRESET_NAME" \
            -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
            -DVERSION_SUFFIX="$VERSION_SUFFIX" \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="12.0"

        # Build
        BUILD_STATUS=""
        cmake --build --preset "$PRESET_NAME" -j "$JOBS" 2>build.log || BUILD_STATUS=$?
        if [ -n "$BUILD_STATUS" ]; then
            tail -30 build.log
            exit 1
        fi

        # Install config template
        cmake --build --preset "$PRESET_NAME" --target install-conf 2>>build.log || true

        # 2. Locate built binary and install files
        BUILD_DIR="build/$PRESET_NAME"
        DAEMON_BIN="$NZBGET_ROOT/$BUILD_DIR/nzbget"
        
        if [ ! -f "$DAEMON_BIN" ]; then
            # Try with configuration suffix for multi-config generators
            DAEMON_BIN="$NZBGET_ROOT/$BUILD_DIR/$CMAKE_BUILD_TYPE/nzbget"
        fi
        
        if [ ! -f "$DAEMON_BIN" ]; then
            echo "ERROR: nzbget binary not found at $DAEMON_BIN"
            exit 1
        fi

        strip -x "$DAEMON_BIN"
        cp "$DAEMON_BIN" "$STAGING_DIR/bin/nzbget"

        # Copy webui, doc and config
        if [ -d "$NZBGET_ROOT/webui" ]; then
            cp -r "$NZBGET_ROOT/webui" "$STAGING_DIR/share/nzbget/"
        fi
        mkdir -p "$STAGING_DIR/share/nzbget/doc"
        cp "$NZBGET_ROOT/ChangeLog.md" "$NZBGET_ROOT/COPYING" "$STAGING_DIR/share/nzbget/doc/" 2>/dev/null || true
        
        if [ -f "$NZBGET_ROOT/$BUILD_DIR/nzbget.conf" ]; then
            cp "$NZBGET_ROOT/$BUILD_DIR/nzbget.conf" "$STAGING_DIR/share/nzbget/nzbget.conf"
        elif [ -f "$NZBGET_ROOT/nzbget.conf" ]; then
            cp "$NZBGET_ROOT/nzbget.conf" "$STAGING_DIR/share/nzbget/nzbget.conf"
        else
            # Generate default config if not found
            "$DAEMON_BIN" --printconfig > "$STAGING_DIR/share/nzbget/nzbget.conf"
        fi
        
        adjust_conf "$STAGING_DIR/share/nzbget/nzbget.conf"

        # 3. Fetch unpackers into dist/bin
        fetch_unpackers "$ARCH" "$STAGING_DIR/bin"

        echo "Artifacts ready in: $STAGING_DIR/bin"
        ls -la "$STAGING_DIR/bin"

        # 4. Packaging (App / Zip)
        ARCHIVE_NAME="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-$ARCH$SUFFIX.zip"
        if [[ "$TARGETS" == *"app"* ]] || [[ "$TARGETS" == *"package"* ]] || [[ "$TARGETS" == *"all"* ]]; then
            if has_xcodebuild; then
                echo "Building macOS GUI App ($TARGET_NAME)..."
                # The xcodeproj is at platforms/macos/NZBGet.xcodeproj
                # It expects daemon payload at platforms/macos/Resources/daemon/usr/local
                APP_DAEMON_ROOT="$NZBGET_ROOT/platforms/macos/Resources/daemon/usr/local"
                rm -rf "$APP_DAEMON_ROOT"
                mkdir -p "$APP_DAEMON_ROOT/bin" "$APP_DAEMON_ROOT/share"
                cp "$DAEMON_BIN" "$APP_DAEMON_ROOT/bin/nzbget"
                # Also copy unpackers and cert
                cp "$STAGING_DIR/bin/7za" "$APP_DAEMON_ROOT/bin/" 2>/dev/null || true
                cp "$STAGING_DIR/bin/unrar" "$APP_DAEMON_ROOT/bin/" 2>/dev/null || true
                cp "$STAGING_DIR/bin/cacert.pem" "$APP_DAEMON_ROOT/bin/" 2>/dev/null || true
                # Copy share (webui, doc, nzbget.conf)
                cp -r "$STAGING_DIR/share/nzbget" "$APP_DAEMON_ROOT/share/"

                cd "$STAGING_DIR"
                xcodebuild -project "$NZBGET_ROOT/platforms/macos/NZBGet.xcodeproj" -configuration "Release" -destination "platform=macOS" build >xcodebuild.log 2>&1 || {
                    tail -30 xcodebuild.log
                    exit 1
                }
                # Find the built app (xcodebuild puts it in platforms/macos/build/Release)
                APP_PATH=$(find "$NZBGET_ROOT/platforms/macos/build" -name "NZBGet.app" -type d 2>/dev/null | head -1)
                if [ -n "$APP_PATH" ]; then
                    (cd "$(dirname "$APP_PATH")" && zip -r "$ARCHIVE_NAME" "$(basename "$APP_PATH")" >/dev/null)
                    mv "$(dirname "$APP_PATH")/$ARCHIVE_NAME" "$DIST_BASE/$ARCHIVE_NAME"
                    echo "App bundle archive created: $DIST_BASE/$ARCHIVE_NAME"
                else
                    echo "ERROR: NZBGet.app not found after xcodebuild"
                    exit 1
                fi
            else
                echo "NOTE: Full Xcode.app not active (CommandLineTools active). Packaging binary distribution..."
                (cd "$STAGING_DIR" && zip -r "$ARCHIVE_NAME" bin share >/dev/null)
                mv "$STAGING_DIR/$ARCHIVE_NAME" "$DIST_BASE/$ARCHIVE_NAME"
                echo "Binary distribution archive created: $DIST_BASE/$ARCHIVE_NAME"
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

        UNI_ARCHIVE="nzbget-$VERSION$VERSION_SUFFIX-bin-macos-universal$SUFFIX.zip"
        if [[ "$TARGETS" == *"app"* ]] || [[ "$TARGETS" == *"package"* ]] || [[ "$TARGETS" == *"all"* ]]; then
            if has_xcodebuild && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64$SUFFIX.zip" ] && [ -f "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-arm64$SUFFIX.zip" ]; then
                UNI_TMP="$NZBGET_ROOT/build/universal-app-tmp"
                rm -rf "$UNI_TMP"
                mkdir -p "$UNI_TMP"
                cd "$UNI_TMP"
                unzip -qo "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64$SUFFIX.zip"
                mv NZBGet.app NZBGet.x64.app
                unzip -qo "$DIST_BASE/nzbget-$VERSION$VERSION_SUFFIX-bin-macos-arm64$SUFFIX.zip"
                mv NZBGet.app NZBGet.arm64.app

                DAEMON_REL="Contents/Resources/daemon/usr/local/bin"
                lipo -create "NZBGet.x64.app/$DAEMON_REL/nzbget" "NZBGet.arm64.app/$DAEMON_REL/nzbget" -output "$UNI_TMP/nzbget"
                lipo -create "NZBGet.x64.app/$DAEMON_REL/unrar" "NZBGet.arm64.app/$DAEMON_REL/unrar" -output "$UNI_TMP/unrar"
                
                mv NZBGet.arm64.app NZBGet.app
                mv "$UNI_TMP/nzbget" "NZBGet.app/$DAEMON_REL/nzbget"
                mv "$UNI_TMP/unrar" "NZBGet.app/$DAEMON_REL/unrar"

                zip -r "$UNI_ARCHIVE" NZBGet.app >/dev/null
                mv "$UNI_ARCHIVE" "$DIST_BASE/$UNI_ARCHIVE"
                rm -rf "$UNI_TMP"
                echo "Universal app archive created: $DIST_BASE/$UNI_ARCHIVE"
            else
                echo "Packaging Universal binary distribution archive..."
                (cd "$UNI_DIR" && zip -r "$UNI_ARCHIVE" bin share >/dev/null)
                mv "$UNI_DIR/$UNI_ARCHIVE" "$DIST_BASE/$UNI_ARCHIVE"
                echo "Universal binary archive created: $DIST_BASE/$UNI_ARCHIVE"
            fi
        fi
        cd "$NZBGET_ROOT"
    fi
done

echo "Done."
echo "Artifacts are located in: $DIST_BASE"