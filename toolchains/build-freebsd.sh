#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2015-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

# FreeBSD sysroot parameters
# Usage: build-freebsd.sh [amd64|arm64|x86_64|aarch64] [version]
# Output: <repo>/build/toolchains/freebsd/sysroot (amd64) or <repo>/build/toolchains/freebsd/sysroot-aarch64 (arm64)

SYSROOT_ARCH="${1:-amd64}"
FREEBSD_VERSION="${2:-13.0}"

# strict error handling: never continue with a broken sysroot
set -e

case "$SYSROOT_ARCH" in
    amd64|x86_64)
        SYSROOT_ARCH="amd64"
        REL_PATH="amd64"
        SYSROOT_NAME="sysroot"
        ;;
    arm64|aarch64)
        SYSROOT_ARCH="arm64"
        REL_PATH="arm64/aarch64"
        SYSROOT_NAME="sysroot-aarch64"
        ;;
    *)
        echo "Unknown architecture: $SYSROOT_ARCH (expected amd64/x86_64 or arm64/aarch64)"
        exit 1
        ;;
esac

URL_CURRENT="https://download.freebsd.org/releases/$REL_PATH/$FREEBSD_VERSION-RELEASE/base.txz"
URL_ARCHIVE="https://archive.freebsd.org/old-releases/$REL_PATH/$FREEBSD_VERSION-RELEASE/base.txz"
URL_FTP_ARCHIVE="http://ftp-archive.freebsd.org/pub/FreeBSD-Archive/old-releases/$SYSROOT_ARCH/$FREEBSD_VERSION-RELEASE/base.txz"

### START OF THE SCRIPT

# resolve script and repository location
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

ROOTDIR=${FREEBSD_PREFIX:-$NZBGET_ROOT/build/toolchains/freebsd}
mkdir -p "$ROOTDIR"

# Download the FreeBSD base image (a partial or corrupt download is never cached)
SRCDIR=${SRCDIR:-$NZBGET_ROOT/build/dl}
FREEBSD_BASE="base-$SYSROOT_ARCH-$FREEBSD_VERSION.txz"
mkdir -p "$SRCDIR"

# integrity check of a downloaded image before it becomes the cached copy
verify_base() {
    if command -v xz >/dev/null 2>&1; then
        xz -t "$1"
    else
        tar -tf "$1" >/dev/null
    fi
}

# fetch one url into the cache; any failure removes the partial file
fetch_base() {
    rm -f "$SRCDIR/$FREEBSD_BASE.part"
    if ! curl -fL -o "$SRCDIR/$FREEBSD_BASE.part" "$1"; then
        rm -f "$SRCDIR/$FREEBSD_BASE.part"
        return 1
    fi
    if ! verify_base "$SRCDIR/$FREEBSD_BASE.part"; then
        echo "ERROR: downloaded image is incomplete or corrupt"
        rm -f "$SRCDIR/$FREEBSD_BASE.part"
        return 1
    fi
    mv "$SRCDIR/$FREEBSD_BASE.part" "$SRCDIR/$FREEBSD_BASE"
}

# current release location first, FreeBSD archive as fallback for old versions
download_base() {
    echo "Downloading FreeBSD $FREEBSD_VERSION base image ($SYSROOT_ARCH)"
    if fetch_base "$URL_CURRENT"; then
        return 0
    fi
    echo "  primary source failed, trying FreeBSD HTTPS archive"
    if fetch_base "$URL_ARCHIVE"; then
        return 0
    fi
    echo "  HTTPS archive failed, trying FreeBSD FTP archive"
    if fetch_base "$URL_FTP_ARCHIVE"; then
        return 0
    fi
    echo "ERROR: unable to download FreeBSD base image from any source"
    exit 1
}

if [ ! -f "$SRCDIR/$FREEBSD_BASE" ]; then
    download_base
fi

SYSROOT="$ROOTDIR/$SYSROOT_NAME"
# never leave a half-built sysroot behind: it would be auto-detected as a valid toolchain
trap 'rm -rf "$SYSROOT"; echo "ERROR: build failed - incomplete sysroot removed"' ERR
rm -rf "$SYSROOT"
mkdir -p "$SYSROOT"

# A cached image that fails to extract is corrupt: drop it and download it again once.
EXTRACT_OK=0
for ATTEMPT in 1 2; do
    if tar -xf "$SRCDIR/$FREEBSD_BASE" -C "$SYSROOT" ./lib/ ./usr/lib/ ./usr/include/; then
        EXTRACT_OK=1
        break
    fi
    rm -rf "$SYSROOT"
    mkdir -p "$SYSROOT"
    rm -f "$SRCDIR/$FREEBSD_BASE"
    if [ "$ATTEMPT" -eq 1 ]; then
        echo "ERROR: cached image is corrupt or incomplete - downloading it again"
        download_base
    fi
done
if [ "$EXTRACT_OK" -ne 1 ]; then
    rm -rf "$SYSROOT"
    echo "ERROR: failed to extract $SRCDIR/$FREEBSD_BASE"
    exit 1
fi

cd "$SYSROOT/usr/lib"
# Fix symlinks pointing outside the sysroot (absolute /lib/... targets).
while IFS= read -r LINK; do
    TARGET=$(readlink "$LINK" || true)
    case "$TARGET" in
        /lib/*) ln -sf "$SYSROOT$TARGET" "$LINK" ;;
    esac
done < <(find . -type l)
ln -sf libc++.a "$SYSROOT/usr/lib/libstdc++.a"
ln -sf libc++.so "$SYSROOT/usr/lib/libstdc++.so"
trap - ERR

# pack archive for distribution
mkdir -p "$NZBGET_ROOT/build/toolchains"
TARBALL="freebsd-$SYSROOT_NAME.tar.gz"
tar -czf "$NZBGET_ROOT/build/toolchains/$TARBALL" -C "$ROOTDIR" "$SYSROOT_NAME"

echo "Done."
echo "  sysroot: $SYSROOT"
echo "  archive: $NZBGET_ROOT/build/toolchains/$TARBALL"
