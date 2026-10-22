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

# FreeBSD 13.0 cross toolchain for NZBGet
#
#   1. Base sysroot: FreeBSD 13.0 libc/libthr/libm and OS headers from the
#      pinned nzbgetcom/build-files release assets (immutable, SHA-256 verified).
#      The official FreeBSD 13.x distributions are no longer downloadable
#      (FreeBSD 13 is EOL, archive.freebsd.org is unreachable), so the sysroot
#      comes from our own verified mirror instead of base.txz.
#   2. C++ runtime: static libc++/libc++abi/libunwind and compiler-rt builtins
#      built from the pinned llvm-project sources. The base sysroot ships
#      libc++ 11, which lacks <coroutine>, <format>, <span> and <concepts>.
#
# Usage: build-toolchain.sh [amd64|arm64|x86_64|aarch64] [version]
#
# Requires clang/clang++ (override with CC/CXX), llvm-ar, llvm-ranlib, llvm-nm,
# cmake and ninja in PATH. The runtime is built with the same LLVM version as
# the clang used by the FreeBSD CI job (LLVM_VERSION below).
#
# Output:
#   build/toolchains/freebsd/sysroot[-aarch64]   base sysroot
#   build/toolchains/freebsd/libcxx[-aarch64]    static C++ runtime

SYSROOT_ARCH="${1:-amd64}"
FREEBSD_VERSION="${2:-13.0}"

LLVM_VERSION="19.1.7"
LLVM_ASSET="llvm-project-$LLVM_VERSION.src.tar.xz"
LLVM_SHA256="82401fea7b79d0078043f7598b835284d6650a75b93e64b6f761ea7b63097501"
LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/$LLVM_ASSET"

set -e

die() {
    echo "ERROR: $*" >&2
    exit 1
}

case "$SYSROOT_ARCH" in
    amd64|x86_64)
        SYSROOT_ARCH="amd64"
        SYSROOT_NAME="sysroot"
        LIBCXX_NAME="libcxx"
        TARGET_TRIPLE="x86_64-pc-freebsd13"
        SYSROOT_URL="https://github.com/nzbgetcom/build-files/releases/download/v1.0/freebsd-sysroot.tar.gz"
        SYSROOT_SHA256="7bbf36b593e57f49667c15829f05535a1c6389e25afa7266c61dbe303802c150"
        ;;
    arm64|aarch64)
        SYSROOT_ARCH="arm64"
        SYSROOT_NAME="sysroot-aarch64"
        LIBCXX_NAME="libcxx-aarch64"
        TARGET_TRIPLE="aarch64-pc-freebsd13"
        SYSROOT_URL="https://github.com/nzbgetcom/build-files/releases/download/v8.0/freebsd-sysroot-aarch64.tar.gz"
        SYSROOT_SHA256="73a05832dfab487dc38fcdf023c6705f6e764a6933c34fd06ab262ed1a0cc226"
        ;;
    *)
        die "unknown architecture: $SYSROOT_ARCH (expected amd64/x86_64 or arm64/aarch64)"
        ;;
esac

if [ "$FREEBSD_VERSION" != "13.0" ]; then
    die "only the FreeBSD 13.0 base sysroot is pinned and verified (got $FREEBSD_VERSION)"
fi

# resolve script and repository location
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

ROOTDIR="${FREEBSD_PREFIX:-$NZBGET_ROOT/build/toolchains/freebsd}"
SRCDIR="${SRCDIR:-$NZBGET_ROOT/build/dl}"
OBJDIR="${OBJDIR:-$NZBGET_ROOT/build/obj}"
SYSROOT="$ROOTDIR/$SYSROOT_NAME"
# Builder image names the x86_64 tree sysroot-x86_64. Local installs use "sysroot".
if [ ! -d "$SYSROOT/usr/include" ] && [ -d "$ROOTDIR/sysroot-x86_64/usr/include" ]; then
    SYSROOT="$ROOTDIR/sysroot-x86_64"
fi
LIBCXX_DIR="$ROOTDIR/$LIBCXX_NAME"
mkdir -p "$ROOTDIR" "$SRCDIR" "$OBJDIR"

if command -v sha256sum >/dev/null 2>&1; then
    SHA256_CMD="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
    SHA256_CMD="shasum -a 256"
else
    die "no SHA-256 utility found (need sha256sum or shasum)"
fi

verify_sha256() {
    local actual
    actual=$($SHA256_CMD "$1" | awk '{print $1}')
    if [ "$actual" != "$2" ]; then
        echo "ERROR: SHA-256 checksum mismatch for $1" >&2
        echo "  Expected: $2" >&2
        echo "  Actual:   $actual" >&2
        return 1
    fi
    echo "  SHA-256 checksum verified: $actual"
}

# Reuse a cached archive after re-verifying it, download and verify it otherwise
ensure_asset() {
    local url="$1"
    local dest="$2"
    local sha256="$3"
    if [ -f "$dest" ]; then
        if verify_sha256 "$dest" "$sha256"; then
            return 0
        fi
        echo "WARNING: cached $(basename "$dest") failed the checksum, re-downloading..."
        rm -f "$dest"
    fi
    echo "Downloading $(basename "$dest")"
    curl -fL --retry 3 -o "$dest.part" "$url" || { rm -f "$dest.part"; die "download failed: $url"; }
    verify_sha256 "$dest.part" "$sha256" || { rm -f "$dest.part"; die "checksum mismatch: $url"; }
    mv "$dest.part" "$dest"
}

# 1. FreeBSD base sysroot (libc, libthr, libm, OS headers)
install_sysroot() {
    if [ -d "$SYSROOT/usr/include" ] && [ -d "$SYSROOT/lib" ] && [ -d "$SYSROOT/usr/lib" ]; then
        echo "FreeBSD $FREEBSD_VERSION base sysroot already present ($SYSROOT)"
        return 0
    fi

    local asset="$SRCDIR/freebsd-base-$SYSROOT_NAME.tar.gz"
    ensure_asset "$SYSROOT_URL" "$asset" "$SYSROOT_SHA256"

    local tmp="$ROOTDIR/.$SYSROOT_NAME.tmp"
    rm -rf "$tmp"
    mkdir -p "$tmp"
    trap 'rm -rf "$tmp"' ERR
    tar -xf "$asset" -C "$tmp" --strip-components=1
    # the image has a single dangling absolute symlink (FreeBSD test data);
    # dropping absolute links keeps the sysroot relocatable
    find "$tmp" -type l -lname '/*' -delete
    trap - ERR

    rm -rf "$SYSROOT"
    mv "$tmp" "$SYSROOT"
    echo "  sysroot: $SYSROOT"
}

# 2. Static libc++/libc++abi/libunwind and compiler-rt builtins
build_libcxx() {
    if [ -f "$LIBCXX_DIR/lib/libc++.a" ] && [ -f "$LIBCXX_DIR/lib/libc++abi.a" ] \
        && [ -f "$LIBCXX_DIR/lib/libunwind.a" ] && [ -f "$LIBCXX_DIR/lib/libclang_rt.builtins.a" ] \
        && [ -d "$LIBCXX_DIR/include/c++/v1" ]; then
        echo "Static C++ runtime already present ($LIBCXX_DIR)"
        return 0
    fi

    local cc="${CC:-clang}"
    local cxx="${CXX:-clang++}"
    local ar="${LLVM_AR:-llvm-ar}"
    local ranlib="${LLVM_RANLIB:-llvm-ranlib}"
    local nm="${LLVM_NM:-llvm-nm}"
    local tool
    for tool in "$cc" "$cxx" "$ar" "$ranlib" "$nm" cmake ninja; do
        command -v "$tool" >/dev/null 2>&1 || die "$tool not found in PATH (required for the static C++ runtime)"
    done
    # CMake resolves relative tool names against the project root, so use absolute paths
    cc=$(command -v "$cc")
    cxx=$(command -v "$cxx")
    ar=$(command -v "$ar")
    ranlib=$(command -v "$ranlib")
    nm=$(command -v "$nm")

    local tarball="$SRCDIR/$LLVM_ASSET"
    ensure_asset "$LLVM_URL" "$tarball" "$LLVM_SHA256"

    local llvm_src="$OBJDIR/llvm-project-$LLVM_VERSION"
    if [ ! -d "$llvm_src/runtimes" ]; then
        echo "Extracting llvm-project $LLVM_VERSION sources"
        rm -rf "$llvm_src"
        tar -xf "$tarball" -C "$OBJDIR"
        mv "$OBJDIR/llvm-project-$LLVM_VERSION.src" "$llvm_src"
    fi

    local -a cross_args=(
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_SYSTEM_NAME=FreeBSD
        -DCMAKE_SYSTEM_VERSION="$FREEBSD_VERSION"
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
        -DCMAKE_SYSROOT="$SYSROOT"
        -DCMAKE_C_COMPILER="$cc"
        -DCMAKE_CXX_COMPILER="$cxx"
        -DCMAKE_ASM_COMPILER="$cc"
        -DCMAKE_C_COMPILER_TARGET="$TARGET_TRIPLE"
        -DCMAKE_CXX_COMPILER_TARGET="$TARGET_TRIPLE"
        -DCMAKE_ASM_COMPILER_TARGET="$TARGET_TRIPLE"
        -DCMAKE_AR="$ar"
        -DCMAKE_RANLIB="$ranlib"
        -DCMAKE_NM="$nm"
    )

    local build_dir="$OBJDIR/freebsd-libcxx-$SYSROOT_ARCH"
    echo "Building static libc++/libc++abi/libunwind for $TARGET_TRIPLE"
    rm -rf "$build_dir"
    cmake -G Ninja -S "$llvm_src/runtimes" -B "$build_dir" "${cross_args[@]}" \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
        -DLIBCXX_ENABLE_STATIC=ON \
        -DLIBCXX_ENABLE_SHARED=OFF \
        -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
        -DLIBCXXABI_ENABLE_STATIC=ON \
        -DLIBCXXABI_ENABLE_SHARED=OFF \
        -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
        -DLIBUNWIND_ENABLE_STATIC=ON \
        -DLIBUNWIND_ENABLE_SHARED=OFF \
        -DLIBCXX_CXX_ABI=libcxxabi \
        -Wno-dev
    ninja -C "$build_dir" cxx cxxabi unwind

    # the FreeBSD 13.0 libcompiler_rt.a predates the outline atomics emitted by
    # modern clang (e.g. __aarch64_ldset8_rel)
    local builtins_dir="$OBJDIR/freebsd-builtins-$SYSROOT_ARCH"
    echo "Building compiler-rt builtins for $TARGET_TRIPLE"
    rm -rf "$builtins_dir"
    cmake -G Ninja -S "$llvm_src/compiler-rt/lib/builtins" -B "$builtins_dir" "${cross_args[@]}" \
        -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=OFF \
        -Wno-dev
    ninja -C "$builtins_dir"

    local builtins
    builtins=$(find "$builtins_dir" -name 'libclang_rt.builtins*.a' | head -n1)
    [ -n "$builtins" ] || die "compiler-rt builtins were not produced for $TARGET_TRIPLE"
    if [ "$SYSROOT_ARCH" = "arm64" ] \
        && ! "$nm" --defined-only "$builtins" | grep -q __aarch64_ldset8_rel; then
        die "compiler-rt builtins for $TARGET_TRIPLE lack __aarch64_ldset8_rel"
    fi

    local tmp="$ROOTDIR/.$LIBCXX_NAME.tmp"
    rm -rf "$tmp"
    mkdir -p "$tmp/lib" "$tmp/include"
    cp "$build_dir/lib/libc++.a" "$build_dir/lib/libc++abi.a" "$build_dir/lib/libunwind.a" "$tmp/lib/"
    cp "$builtins" "$tmp/lib/libclang_rt.builtins.a"
    cp -R "$build_dir/include/c++" "$tmp/include/"
    echo "$LLVM_VERSION" > "$tmp/llvm-version"

    rm -rf "$LIBCXX_DIR"
    mv "$tmp" "$LIBCXX_DIR"
    echo "  libc++: $LIBCXX_DIR (LLVM $LLVM_VERSION, $TARGET_TRIPLE)"
}

install_sysroot
build_libcxx

echo "Done."
echo "  sysroot: $SYSROOT"
echo "  libc++:  $LIBCXX_DIR"
