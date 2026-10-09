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
#  along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# Build a static universal (arm64 + x86_64) libc++/libc++abi for macOS
# (macOS 10.14+ for x86_64, macOS 11.0+ for arm64).
# Compiled with the same upstream LLVM clang that toolchains/macos-llvm.cmake
# uses to build NZBGet itself (one compiler for the runtime and the project).
#
# Usage: bash toolchains/build-macos-libcxx.sh [llvm-tag] [output-prefix]
#   llvm-tag       llvm-project git tag (default: llvmorg-19.1.7)
#   output-prefix  install prefix (default: <repo>/build/toolchains/macos12-libcxx)
#
# Environment:
#   LLVM_SRC       llvm-project source dir (default: <repo>/build/dl/llvm-project)

set -e

# resolve script and repository location
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

LLVM_TAG=${1:-llvmorg-19.1.7}
OUTPUT_PREFIX=${2:-$NZBGET_ROOT/build/toolchains/macos12-libcxx}
SOURCE_DIR="$NZBGET_ROOT/build/dl"
OBJ_BASE="$NZBGET_ROOT/build/obj"
LLVM_SRC=${LLVM_SRC:-$SOURCE_DIR/llvm-project}
DEPLOYMENT_TARGET_X86_64=${MACOSX_DEPLOYMENT_TARGET_X86_64:-10.14}
DEPLOYMENT_TARGET_ARM64=${MACOSX_DEPLOYMENT_TARGET_ARM64:-11.0}

if [ "$(uname -s)" != "Darwin" ]; then
	echo "ERROR: this script must be run on macOS"
	exit 1
fi

for tool in git cmake ninja lipo; do
	if ! command -v $tool >/dev/null 2>&1; then
		echo "ERROR: $tool not found"
		exit 1
	fi
done

# locate the same upstream LLVM clang that toolchains/macos-llvm.cmake uses,
# so libc++ and NZBGet are built with one compiler (zig does the same: its
# bundled clang compiles both its libcxx and the user's code)
LLVM_CC=""
LLVM_CXX=""
if command -v brew >/dev/null 2>&1; then
	BREW_PREFIX=$(brew --prefix llvm 2>/dev/null || true)
	if [ -n "$BREW_PREFIX" ] && [ -x "$BREW_PREFIX/bin/clang" ]; then
		LLVM_CC="$BREW_PREFIX/bin/clang"
		LLVM_CXX="$BREW_PREFIX/bin/clang++"
	fi
fi
if [ -z "$LLVM_CC" ]; then
	for KEG in $(ls -d /opt/homebrew/opt/llvm@* /usr/local/opt/llvm@* 2>/dev/null | sort -t@ -k2 -n -r); do
		if [ -x "$KEG/bin/clang" ]; then
			LLVM_CC="$KEG/bin/clang"
			LLVM_CXX="$KEG/bin/clang++"
			break
		fi
	done
fi
if [ -z "$LLVM_CC" ]; then
	echo "WARNING: upstream LLVM clang not found (brew install llvm);"
	echo "WARNING: falling back to the system compiler. libc++ and NZBGet"
	echo "WARNING: would then be built by different compilers."
else
	echo "Host compiler: $LLVM_CC ($($LLVM_CC --version | head -n1))"
fi

# fetch llvm-project sources
if [ ! -d "$LLVM_SRC/.git" ]; then
	mkdir -p "$(dirname "$LLVM_SRC")"
	git clone --depth=1 --branch $LLVM_TAG https://github.com/llvm/llvm-project.git "$LLVM_SRC"
else
	echo "Using existing sources in $LLVM_SRC (delete it to re-fetch $LLVM_TAG)"
fi

cd "$LLVM_SRC"

# build static libc++ / libc++abi for one architecture
# zig-spec (src/libs/libcxx.zig): fully self-contained runtime with ALL
# symbols hidden, so dyld cannot coalesce them with libc++ in the shared cache:
#   - -fvisibility=hidden: LLVM adds it to libc++ only; libcxxabi needs it too
#   - _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS: turns off the
#     _LIBCPP_VISIBILITY("default") re-annotations (baked into __config_site via
#     LIBCXX_EXTRA_SITE_DEFINES so consumers of the installed headers match)
#   - _LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS: same for libc++abi
#   - _LIBCPP_DISABLE_AVAILABILITY: no Apple availability markup -> std::format
#     and friends work on a macOS 12 deployment target
build_arch () {
	local ARCH=$1
	local TARGET_DEPLOYMENT
	if [ "$ARCH" = "x86_64" ]; then
		TARGET_DEPLOYMENT="$DEPLOYMENT_TARGET_X86_64"
	else
		TARGET_DEPLOYMENT="$DEPLOYMENT_TARGET_ARM64"
	fi
	local COMPILER_ARGS=()
	if [ -n "$LLVM_CC" ]; then
		COMPILER_ARGS=(-DCMAKE_C_COMPILER="$LLVM_CC" -DCMAKE_CXX_COMPILER="$LLVM_CXX")
	fi
	echo "Building libc++ for $ARCH (targeting macOS $TARGET_DEPLOYMENT)..."
	local BUILD_DIR="$OBJ_BASE/libcxx-$ARCH"
	rm -rf "$BUILD_DIR"
	cmake -G Ninja -S runtimes -B "$BUILD_DIR" \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
		-DCMAKE_BUILD_TYPE=Release \
		-Wno-author -Wno-deprecated \
		"${COMPILER_ARGS[@]}" \
		-DCMAKE_OSX_ARCHITECTURES="$ARCH" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET="$TARGET_DEPLOYMENT" \
		-DLIBCXX_ENABLE_STATIC=ON \
		-DLIBCXX_ENABLE_SHARED=OFF \
		-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
		-DLIBCXXABI_ENABLE_STATIC=ON \
		-DLIBCXXABI_ENABLE_SHARED=OFF \
		-DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
		-DLIBCXX_CXX_ABI=libcxxabi \
		-DLIBCXX_EXTRA_SITE_DEFINES="_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS" \
		-DCMAKE_CXX_FLAGS="-D_LIBCPP_DISABLE_AVAILABILITY -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS -fvisibility=hidden -fvisibility-inlines-hidden"
	ninja -C "$BUILD_DIR" cxx cxxabi
}

build_arch arm64
build_arch x86_64

# assemble universal static libraries and headers
echo "Assembling universal libraries in $OUTPUT_PREFIX ..."
mkdir -p $OUTPUT_PREFIX/lib $OUTPUT_PREFIX/include
lipo -create "$OBJ_BASE/libcxx-arm64/lib/libc++.a" "$OBJ_BASE/libcxx-x86_64/lib/libc++.a" -output $OUTPUT_PREFIX/lib/libc++.a
lipo -create "$OBJ_BASE/libcxx-arm64/lib/libc++abi.a" "$OBJ_BASE/libcxx-x86_64/lib/libc++abi.a" -output $OUTPUT_PREFIX/lib/libc++abi.a
rm -rf $OUTPUT_PREFIX/include/c++
cp -R "$OBJ_BASE/libcxx-arm64/include/c++" $OUTPUT_PREFIX/include/

# pack archive for distribution (version derived from the llvm tag)
LLVM_VERSION=${LLVM_TAG#llvmorg-}
TARBALL="macos12-libcxx-llvm$LLVM_VERSION.tar.gz"
mkdir -p "$NZBGET_ROOT/build/toolchains"
tar -czf "$NZBGET_ROOT/build/toolchains/$TARBALL" -C "$(dirname "$OUTPUT_PREFIX")" "$(basename "$OUTPUT_PREFIX")"

echo "Done."
echo "  install: $OUTPUT_PREFIX"
echo "  archive: $NZBGET_ROOT/build/toolchains/$TARBALL"
