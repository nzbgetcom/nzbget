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
# Usage: build-toolchain.sh [amd64|arm64|x86_64|aarch64] [version]
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

# Official SHA-256 checksums from FreeBSD release MANIFESTs:
#   amd64: https://archive.freebsd.org/old-releases/amd64/13.0-RELEASE/MANIFEST
#   arm64: https://archive.freebsd.org/old-releases/arm64/aarch64/13.0-RELEASE/MANIFEST
PINNED_SHA256_amd64_13_0="be7ab3c21ca650b3763ad3605edd18c39d570abd40b0aad3058df64c3e38fb51"
PINNED_SHA256_arm64_13_0="b0f1a4b2530924d91eb8c46a88a6c5d12ff6a79d990d2fe98e0b7370c1b0f13f"

### START OF THE SCRIPT

# resolve script and repository location
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

ROOTDIR=${FREEBSD_PREFIX:-$NZBGET_ROOT/build/toolchains/freebsd}
mkdir -p "$ROOTDIR"

# Download the FreeBSD base image (a partial or corrupt download is never cached)
SRCDIR=${SRCDIR:-$NZBGET_ROOT/build/dl}
FREEBSD_BASE="base-$SYSROOT_ARCH-$FREEBSD_VERSION.txz"
mkdir -p "$SRCDIR"

compute_sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    elif command -v sha256 >/dev/null 2>&1; then
        sha256 -q "$1"
    else
        echo "ERROR: no SHA-256 utility found (need sha256sum, shasum, or sha256)" >&2
        return 1
    fi
}

get_expected_sha256() {
    local key="PINNED_SHA256_${SYSROOT_ARCH}_$(echo "$FREEBSD_VERSION" | tr '.' '_')"
    eval local pinned="\$$key"
    if [ -n "$pinned" ]; then
        echo "$pinned"
        return 0
    fi

    # Fallback for other versions: query official HTTPS MANIFEST
    local manifest_url="https://download.freebsd.org/releases/$REL_PATH/$FREEBSD_VERSION-RELEASE/MANIFEST"
    local sha
    sha=$(curl -fsSL "$manifest_url" 2>/dev/null | awk '$1 == "base.txz" { print $2 }')
    if [ -n "$sha" ]; then
        echo "$sha"
        return 0
    fi

    manifest_url="https://archive.freebsd.org/old-releases/$REL_PATH/$FREEBSD_VERSION-RELEASE/MANIFEST"
    sha=$(curl -fsSL "$manifest_url" 2>/dev/null | awk '$1 == "base.txz" { print $2 }')
    if [ -n "$sha" ]; then
        echo "$sha"
        return 0
    fi

    return 1
}

# integrity and authenticity check of a downloaded image
verify_base() {
    local file="$1"

    # 1. Structural check
    if command -v xz >/dev/null 2>&1; then
        xz -t "$file" >/dev/null 2>&1 || return 1
    else
        tar -tf "$file" >/dev/null 2>&1 || return 1
    fi

    # 2. Cryptographic SHA-256 verification
    local expected_sha
    expected_sha=$(get_expected_sha256)
    if [ -z "$expected_sha" ]; then
        echo "ERROR: could not determine expected SHA-256 checksum for FreeBSD $FREEBSD_VERSION ($SYSROOT_ARCH)" >&2
        return 1
    fi

    local actual_sha
    actual_sha=$(compute_sha256 "$file")
    if [ "$actual_sha" != "$expected_sha" ]; then
        echo "ERROR: SHA-256 checksum mismatch for $file" >&2
        echo "  Expected: $expected_sha" >&2
        echo "  Actual:   $actual_sha" >&2
        return 1
    fi

    echo "  SHA-256 checksum verified: $actual_sha"
    return 0
}

# fetch one url into the cache; any failure removes the partial file
fetch_base() {
    rm -f "$SRCDIR/$FREEBSD_BASE.part"
    if ! curl -fL -o "$SRCDIR/$FREEBSD_BASE.part" "$1"; then
        rm -f "$SRCDIR/$FREEBSD_BASE.part"
        return 1
    fi
    if ! verify_base "$SRCDIR/$FREEBSD_BASE.part"; then
        echo "ERROR: downloaded image is incomplete, corrupt, or failed checksum"
        rm -f "$SRCDIR/$FREEBSD_BASE.part"
        return 1
    fi
    mv "$SRCDIR/$FREEBSD_BASE.part" "$SRCDIR/$FREEBSD_BASE"
}

# current release location first, FreeBSD HTTPS archive as fallback for old versions
download_base() {
    echo "Downloading FreeBSD $FREEBSD_VERSION base image ($SYSROOT_ARCH)"
    if fetch_base "$URL_CURRENT"; then
        return 0
    fi
    echo "  primary source failed, trying FreeBSD HTTPS archive"
    if fetch_base "$URL_ARCHIVE"; then
        return 0
    fi
    echo "ERROR: unable to download verified FreeBSD base image from HTTPS sources"
    exit 1
}

if [ -f "$SRCDIR/$FREEBSD_BASE" ]; then
    echo "Verifying cached FreeBSD base image ($FREEBSD_BASE)..."
    if ! verify_base "$SRCDIR/$FREEBSD_BASE"; then
        echo "WARNING: cached image is corrupt or failed checksum, re-downloading..."
        rm -f "$SRCDIR/$FREEBSD_BASE"
        download_base
    fi
else
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

# Fix symlinks pointing outside the sysroot (absolute targets starting with /).
# Convert them to sysroot-relative paths so the sysroot remains 100% relocatable.
while IFS= read -r LINK; do
    TARGET=$(readlink "$LINK" || true)
    case "$TARGET" in
        /*)
            LINK_DIR=$(dirname "$LINK")
            REL_LINK_DIR="${LINK_DIR#"$SYSROOT"}"
            clean_dir="${REL_LINK_DIR#/}"
            up=""
            if [ -n "$clean_dir" ]; then
                depth=$(awk -F/ '{print NF}' <<< "$clean_dir")
                for ((i=0; i<depth; i++)); do up="../$up"; done
            fi
            rel_target="${up}${TARGET#/}"
            ln -sf "$rel_target" "$LINK"
            ;;
    esac
done < <(find "$SYSROOT" -type l)

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
