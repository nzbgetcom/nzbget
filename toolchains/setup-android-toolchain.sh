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
# Prepare the complete, hermetic Android API 21+ cross toolchain:
#   1. LLVM Clang/LLD 19.1.7  (official release tarball)
#   2. Bionic sysroot         (pinned Android NDK, sysroot only)
#   3. compiler-rt builtins + static libc++/libc++abi/libunwind
#      (built from llvm-project 19.1.7 sources for every architecture)
#
# Usage: bash toolchains/setup-android-toolchain.sh [arm64|armv7|x86_64|x86|all]...
#
# Output (under $NZBGET_ANDROID_TOOLCHAIN_DIR, default <repo>/build/toolchains):
#   android-llvm/      clang, lld, llvm-ar/ranlib/strip + builtins in the resource dir
#   android-sysroot/   Bionic headers and libraries
#   android-libcxx/    <arch>/lib/{libc++,libc++abi,libunwind}.a and include/c++/v1
#
# Every stage is idempotent: existing results are reused.
#

set -euo pipefail

LLVM_VERSION="19.1.7"
LLVM_TAG="llvmorg-${LLVM_VERSION}"
# Pinned immutable commit SHA for llvmorg-19.1.7 (GPG-signed by Tobias Hieta <tobias@hieta.se>)
LLVM_COMMIT="cd708029e0b2869e80abe31ddb175f7c35361f90"
NDK_VERSION="${NZBGET_ANDROID_NDK_VERSION:-r26b}"
API_LEVEL="${ANDROID_API_LEVEL:-${NZBGET_ANDROID_API_LEVEL:-21}}"

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

TOOLCHAIN_DIR="${NZBGET_ANDROID_TOOLCHAIN_DIR:-$NZBGET_ROOT/build/toolchains}"
DL_DIR="$NZBGET_ROOT/build/dl"
OBJ_DIR="$NZBGET_ROOT/build/obj"
LLVM_DIR="$TOOLCHAIN_DIR/android-llvm"
SYSROOT_DIR="$TOOLCHAIN_DIR/android-sysroot"
LIBCXX_DIR="$TOOLCHAIN_DIR/android-libcxx"
API_LEVEL_FILE="$LIBCXX_DIR/android-api-level"
LEGACY_API_LEVEL_FILE="$TOOLCHAIN_DIR/android-api-level"
LLVM_SRC="${LLVM_SRC:-$DL_DIR/llvm-project}"

ALL_ARCHS="arm64 armv7 x86_64 x86"
ARCHS=""
while [ $# -gt 0 ]; do
	case "$1" in
		all) ARCHS="$ALL_ARCHS" ;;
		arm64|armv7|x86_64|x86) ARCHS="$ARCHS $1" ;;
		--api-level=*) API_LEVEL="${1#*=}" ;;
		-a|--api-level)
			shift
			if [ $# -eq 0 ]; then
				echo "ERROR: --api-level requires an integer argument"
				exit 1
			fi
			API_LEVEL="$1"
			;;
		*)
			echo "Usage: bash toolchains/setup-android-toolchain.sh [--api-level=<num>] [arm64|armv7|x86_64|x86|all]..."
			exit 1
			;;
	esac
	shift
done
ARCHS="${ARCHS:-$ALL_ARCHS}"

for tool in curl tar unzip cmake ninja git; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "ERROR: required tool not found: $tool"
		exit 1
	fi
done

mkdir -p "$TOOLCHAIN_DIR" "$DL_DIR" "$OBJ_DIR"

# 1. LLVM Clang/LLD from the official release
case "$(uname -s)-$(uname -m)" in
	Linux-x86_64)
		LLVM_ASSET="LLVM-${LLVM_VERSION}-Linux-X64"
		LLVM_SHA256="4a5ec53951a584ed36f80240f6fbf8fdd46b4cf6c7ee87cc2d5018dc37caf679"
		NDK_HOST="linux"
		NDK_SHA256="ad73c0370f0b0a87d1671ed2fd5a9ac9acfd1eb5c43a7fbfbd330f85d19dd632"
		;;
	Darwin-arm64)
		LLVM_ASSET="LLVM-${LLVM_VERSION}-macOS-ARM64"
		LLVM_SHA256="d93bf12952d89fe4ec7501c40475718b722407da6a8d651f05c995863468e570"
		NDK_HOST="darwin"
		NDK_SHA256="4b0ea6148a9a2337e62a0c0c7ac59ff1edc38d69b81d9c58251897d23f7fa321"
		;;
	Darwin-x86_64)
		LLVM_ASSET="LLVM-${LLVM_VERSION}-macOS-X64"
		LLVM_SHA256="49405e75fbe7ad6f8139a33f59ec8c5112b75b3027405c7b92d19f4c6f02c78a"
		NDK_HOST="darwin"
		NDK_SHA256="4b0ea6148a9a2337e62a0c0c7ac59ff1edc38d69b81d9c58251897d23f7fa321"
		;;
	*)
		echo "ERROR: unsupported build host: $(uname -s)-$(uname -m)"
		exit 1
		;;
esac

verify_sha256() {
	local file=$1
	local expected=$2
	local actual=""
	if command -v sha256sum >/dev/null 2>&1; then
		actual=$(sha256sum "$file" | awk '{print $1}')
	elif command -v shasum >/dev/null 2>&1; then
		actual=$(shasum -a 256 "$file" | awk '{print $1}')
	else
		echo "ERROR: neither sha256sum nor shasum is available for checksum verification"
		return 1
	fi
	if [ "$actual" != "$expected" ]; then
		echo "ERROR: SHA256 checksum mismatch for $file"
		echo "  Expected: $expected"
		echo "  Actual:   $actual"
		return 1
	fi
}

llvm_ready() {
	[ -x "$LLVM_DIR/bin/clang" ] && [ -x "$LLVM_DIR/bin/ld.lld" ] \
		&& "$LLVM_DIR/bin/clang" --version 2>/dev/null | grep -q "clang version ${LLVM_VERSION}"
}

setup_llvm() {
	if llvm_ready; then
		echo "LLVM ${LLVM_VERSION}: using $LLVM_DIR"
		return
	fi

	local tarball="$DL_DIR/${LLVM_ASSET}.tar.xz"
	if [ -f "$tarball" ]; then
		if ! verify_sha256 "$tarball" "$LLVM_SHA256"; then
			echo "Warning: cached LLVM archive failed checksum check, re-downloading..."
			rm -f "$tarball"
		fi
	fi
	if [ ! -f "$tarball" ]; then
		echo "Downloading LLVM ${LLVM_VERSION} (${LLVM_ASSET})..."
		curl -fL --retry 3 -o "$tarball.part" \
			"https://github.com/llvm/llvm-project/releases/download/${LLVM_TAG}/${LLVM_ASSET}.tar.xz"
		verify_sha256 "$tarball.part" "$LLVM_SHA256"
		mv "$tarball.part" "$tarball"
	fi

	echo "Extracting required LLVM tools..."
	rm -rf "$LLVM_DIR"
	mkdir -p "$LLVM_DIR"
	local members=""
	for name in clang-19 clang clang++ lld ld.lld llvm-ar llvm-ranlib llvm-objcopy llvm-strip llvm-nm; do
		members="$members $LLVM_ASSET/bin/$name"
	done
	members="$members $LLVM_ASSET/lib/clang/19/include"
	# shellcheck disable=SC2086
	tar -xJf "$tarball" --strip-components=1 -C "$LLVM_DIR" $members

	if ! llvm_ready; then
		echo "ERROR: LLVM ${LLVM_VERSION} toolchain is not usable in $LLVM_DIR"
		exit 1
	fi
}

# 2. Bionic sysroot from the pinned NDK
setup_sysroot() {
	if [ -d "$SYSROOT_DIR/usr/include" ] && [ -d "$SYSROOT_DIR/usr/lib" ]; then
		echo "Bionic sysroot: using $SYSROOT_DIR"
		return
	fi

	local zip="$DL_DIR/android-ndk-${NDK_VERSION}-${NDK_HOST}.zip"
	if [ -f "$zip" ]; then
		if ! verify_sha256 "$zip" "$NDK_SHA256"; then
			echo "Warning: cached NDK archive failed checksum check, re-downloading..."
			rm -f "$zip"
		fi
	fi
	if [ ! -f "$zip" ]; then
		echo "Downloading Android NDK ${NDK_VERSION} (${NDK_HOST})..."
		curl -fL --retry 3 -o "$zip.part" \
			"https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-${NDK_HOST}.zip"
		verify_sha256 "$zip.part" "$NDK_SHA256"
		mv "$zip.part" "$zip"
	fi

	echo "Extracting Bionic sysroot..."
	local tmp="$OBJ_DIR/android-ndk-extract"
	rm -rf "$tmp" "$SYSROOT_DIR"
	mkdir -p "$tmp"
	unzip -q -o "$zip" "android-ndk-${NDK_VERSION}/toolchains/llvm/prebuilt/*/sysroot/*" -d "$tmp"

	local extracted
	extracted=$(find "$tmp" -type d -name sysroot -path "*/prebuilt/*" -maxdepth 6 | head -n1)
	if [ -z "$extracted" ] || [ ! -d "$extracted/usr/include" ]; then
		echo "ERROR: sysroot not found in NDK ${NDK_VERSION}"
		exit 1
	fi
	mv "$extracted" "$SYSROOT_DIR"
	rm -rf "$tmp"
}

# 3. compiler-rt builtins + libc++/libc++abi/libunwind
fetch_llvm_sources() {
	if [ ! -d "$LLVM_SRC/.git" ]; then
		echo "Fetching llvm-project ${LLVM_TAG} (pinned commit: ${LLVM_COMMIT})..."
		git clone --depth=1 --branch "$LLVM_TAG" https://github.com/llvm/llvm-project.git "$LLVM_SRC"
	fi

	local current_commit
	current_commit=$(git -C "$LLVM_SRC" rev-parse HEAD 2>/dev/null || true)
	if [ "$current_commit" != "$LLVM_COMMIT" ]; then
		echo "ERROR: llvm-project commit mismatch in $LLVM_SRC!"
		echo "  Expected: $LLVM_COMMIT (${LLVM_TAG})"
		echo "  Found:    $current_commit"
		echo "Hermetic integrity check failed. Removing untrusted sources."
		rm -rf "$LLVM_SRC"
		exit 1
	fi
}

arch_triple() {
	case "$1" in
		arm64)  echo "aarch64-linux-android${API_LEVEL}" ;;
		armv7)  echo "armv7a-linux-androideabi${API_LEVEL}" ;;
		x86_64) echo "x86_64-linux-android${API_LEVEL}" ;;
		x86)    echo "i686-linux-android${API_LEVEL}" ;;
	esac
}

arch_flags() {
	case "$1" in
		armv7) echo "-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb" ;;
		x86)   echo "-march=i686" ;;
		*)     echo "" ;;
	esac
}

# Common CMake options for cross-building LLVM runtimes for Android
cross_options() {
	local triple=$1
	echo \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SYSTEM_NAME=Linux \
		-DANDROID=TRUE \
		-DANDROID_NATIVE_API_LEVEL="${API_LEVEL}" \
		-DANDROID_PLATFORM_LEVEL="${API_LEVEL}" \
		-DLLVM_FORCE_SMALLFILE_FOR_ANDROID=ON \
		-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
		-DCMAKE_SYSROOT="$SYSROOT_DIR" \
		-DCMAKE_C_COMPILER="$LLVM_DIR/bin/clang" \
		-DCMAKE_CXX_COMPILER="$LLVM_DIR/bin/clang++" \
		-DCMAKE_ASM_COMPILER="$LLVM_DIR/bin/clang" \
		-DCMAKE_C_COMPILER_TARGET="$triple" \
		-DCMAKE_CXX_COMPILER_TARGET="$triple" \
		-DCMAKE_ASM_COMPILER_TARGET="$triple" \
		-DCMAKE_AR="$LLVM_DIR/bin/llvm-ar" \
		-DCMAKE_RANLIB="$LLVM_DIR/bin/llvm-ranlib" \
		-DCMAKE_NM="$LLVM_DIR/bin/llvm-nm"
}

# Path where Clang expects the builtins library for this target
builtins_path() {
	"$LLVM_DIR/bin/clang" --target="$1" --sysroot="$SYSROOT_DIR" -rtlib=compiler-rt --print-libgcc-file-name
}

build_builtins() {
	local arch=$1 triple=$2 extra=$3
	local dest
	dest=$(builtins_path "$triple")
	if [ -f "$dest" ]; then
		return
	fi

	echo "Building compiler-rt builtins for $arch ($triple)..."
	local build_dir="$OBJ_DIR/android-builtins-$arch"
	rm -rf "$build_dir"

	# shellcheck disable=SC2046
	cmake -G Ninja -S "$LLVM_SRC/compiler-rt/lib/builtins" -B "$build_dir" \
		$(cross_options "$triple") \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
		-DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN=OFF \
		-DCMAKE_C_FLAGS="-fPIC $extra" \
		-DCMAKE_ASM_FLAGS="-fPIC $extra"
	ninja -C "$build_dir"

	local built
	built=$(find "$build_dir" -name 'libclang_rt.builtins*.a' | head -n1)
	if [ -z "$built" ]; then
		echo "ERROR: compiler-rt builtins were not produced for $arch"
		exit 1
	fi
	# i686 has no native 64-bit atomics in the baseline ISA; OpenSSL needs the generic
	# __atomic_* helpers from compiler-rt (other targets inline them)
	if [ "$arch" = "x86" ] \
		&& ! "$LLVM_DIR/bin/llvm-nm" "$built" 2>/dev/null | grep -Eq ' [TW] __atomic_compare_exchange$'; then
		echo "ERROR: compiler-rt builtins for $arch lack __atomic_compare_exchange"
		exit 1
	fi
	mkdir -p "$(dirname "$dest")"
	cp "$built" "$dest"
}

build_libcxx() {
	local arch=$1 triple=$2 extra=$3
	local out="$LIBCXX_DIR/$arch/lib"
	if [ -f "$out/libc++.a" ] && [ -f "$out/libc++abi.a" ] && [ -f "$out/libunwind.a" ] \
		&& [ -d "$LIBCXX_DIR/include/c++/v1" ]; then
		return
	fi

	echo "Building static libc++/libc++abi/libunwind for $arch ($triple)..."
	local build_dir="$OBJ_DIR/android-libcxx-$arch"
	rm -rf "$build_dir"

	local cxx_flags="-D_LIBCPP_DISABLE_AVAILABILITY -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS -fvisibility=hidden -fvisibility-inlines-hidden -fPIC $extra"
	if [ "$arch" = "armv7" ] || [ "$arch" = "x86" ]; then
		cxx_flags="$cxx_flags -U_FILE_OFFSET_BITS -D_FILE_OFFSET_BITS=32"
	fi

	# shellcheck disable=SC2046
	cmake -G Ninja -S "$LLVM_SRC/runtimes" -B "$build_dir" \
		$(cross_options "$triple") \
		-DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
		-DLIBCXX_ENABLE_STATIC=ON \
		-DLIBCXX_ENABLE_SHARED=OFF \
		-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
		-DLIBCXXABI_ENABLE_STATIC=ON \
		-DLIBCXXABI_ENABLE_SHARED=OFF \
		-DLIBCXXABI_USE_LLVM_UNWINDER=ON \
		-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF \
		-DLIBUNWIND_ENABLE_STATIC=ON \
		-DLIBUNWIND_ENABLE_SHARED=OFF \
		-DLIBCXX_CXX_ABI=libcxxabi \
		-DLIBCXX_EXTRA_SITE_DEFINES="_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS" \
		-DCMAKE_CXX_FLAGS="$cxx_flags" \
		-DCMAKE_C_FLAGS="-fPIC $extra" \
		-DCMAKE_ASM_FLAGS="-fPIC $extra" \
		-Wno-dev -Wno-deprecated
	ninja -C "$build_dir" cxx cxxabi unwind

	mkdir -p "$out"
	cp "$build_dir/lib/libc++.a" "$build_dir/lib/libc++abi.a" "$build_dir/lib/libunwind.a" "$out/"

	if [ ! -d "$LIBCXX_DIR/include/c++/v1" ]; then
		mkdir -p "$LIBCXX_DIR/include"
		cp -R "$build_dir/include/c++" "$LIBCXX_DIR/include/"
	fi
}

setup_runtimes() {
	local need=0
	local marker=""
	if [ -f "$API_LEVEL_FILE" ]; then
		marker="$API_LEVEL_FILE"
	elif [ -f "$LEGACY_API_LEVEL_FILE" ]; then
		marker="$LEGACY_API_LEVEL_FILE"
	fi

	if [ -n "$marker" ]; then
		local existing_api
		existing_api=$(tr -d '[:space:]' < "$marker")
		if [ "$existing_api" != "$API_LEVEL" ]; then
			echo "Android API level changed from $existing_api to $API_LEVEL; invalidating previous runtimes..."
			rm -rf "$LIBCXX_DIR" "$OBJ_DIR/android-libcxx-"* "$OBJ_DIR/android-builtins-"*
			need=1
		fi
	else
		need=1
	fi

	for arch in $ARCHS; do
		local triple dest
		triple=$(arch_triple "$arch")
		dest=$(builtins_path "$triple")
		if [ ! -f "$dest" ] || [ ! -f "$LIBCXX_DIR/$arch/lib/libc++.a" ] || [ ! -d "$LIBCXX_DIR/include/c++/v1" ]; then
			need=1
		fi
	done
	if [ "$need" = "0" ]; then
		# Ensure marker file is recorded in both primary and legacy locations
		mkdir -p "$LIBCXX_DIR" "$TOOLCHAIN_DIR"
		echo "$API_LEVEL" > "$API_LEVEL_FILE"
		echo "$API_LEVEL" > "$LEGACY_API_LEVEL_FILE"
		echo "Runtimes: using $LIBCXX_DIR (API $API_LEVEL)"
		return
	fi

	fetch_llvm_sources
	for arch in $ARCHS; do
		local triple extra
		triple=$(arch_triple "$arch")
		extra=$(arch_flags "$arch")
		build_builtins "$arch" "$triple" "$extra"
		build_libcxx "$arch" "$triple" "$extra"
	done
	mkdir -p "$LIBCXX_DIR" "$TOOLCHAIN_DIR"
	echo "$API_LEVEL" > "$API_LEVEL_FILE"
	echo "$API_LEVEL" > "$LEGACY_API_LEVEL_FILE"
}

setup_llvm
setup_sysroot
setup_runtimes

echo "Android toolchain ready (LLVM ${LLVM_VERSION}, NDK ${NDK_VERSION} sysroot, API ${API_LEVEL})"
echo "  llvm:    $LLVM_DIR"
echo "  sysroot: $SYSROOT_DIR"
echo "  libcxx:  $LIBCXX_DIR"
