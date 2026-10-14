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

# Ensure UTF-8 locale so bsdtar and build tools handle non-ASCII filenames in gcc/testsuite
export LANG="${LANG:-C.UTF-8}"
export LC_ALL="${LC_ALL:-C.UTF-8}"

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
NZBGET_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

TOOLCHAIN_DIR="${NZBGET_LINUX_TOOLCHAIN_DIR:-$NZBGET_ROOT/build/toolchains/linux}"
DL_DIR="${SOURCE_DIR:-$NZBGET_ROOT/build/dl}"
MCM_DIR="$NZBGET_ROOT/build/toolchains/musl-cross-make"
PATCH_DIR="$SCRIPT_DIR/patches/musl-1.2.6"

MCM_REPO="https://github.com/richfelker/musl-cross-make"
MCM_COMMIT="227df8b99103f9c59f6570babf892978e293082f"

GCC_VERSION="14.3.0"
BINUTILS_VERSION="2.44"
MUSL_VERSION="1.2.6"

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

ALL_ARCHS="x86_64 aarch64 armhf armel i686 ppc6xx riscv64"

map_target() {
	case "$1" in
		x86_64|amd64)
			echo "x86_64-linux-musl"
			;;
		aarch64|arm64)
			echo "aarch64-linux-musl"
			;;
		armhf)
			echo "arm-linux-musleabihf"
			;;
		armel)
			echo "arm-linux-musleabi"
			;;
		i686|x86)
			echo "i686-linux-musl"
			;;
		ppc6xx|powerpc)
			echo "powerpc-linux-musl"
			;;
		riscv64)
			echo "riscv64-linux-musl"
			;;
		*)
			echo "Unknown architecture: $1" >&2
			exit 1
			;;
	esac
}

map_arch_name() {
	case "$1" in
		x86_64|amd64) echo "x86_64" ;;
		aarch64|arm64) echo "aarch64" ;;
		armhf) echo "armhf" ;;
		armel) echo "armel" ;;
		i686|x86) echo "i686" ;;
		ppc6xx|powerpc) echo "ppc6xx" ;;
		riscv64) echo "riscv64" ;;
		*) echo "$1" ;;
	esac
}

build_arch() {
	local ARCH_RAW="$1"
	local ARCH
	ARCH=$(map_arch_name "$ARCH_RAW")
	local TARGET
	TARGET=$(map_target "$ARCH_RAW")
	local OUTPUT="$TOOLCHAIN_DIR/$ARCH"

	echo "============================================================"
	echo "Building Musl Toolchain: $ARCH ($TARGET)"
	echo "GCC: $GCC_VERSION | Musl: $MUSL_VERSION | Binutils: $BINUTILS_VERSION"
	echo "Output: $OUTPUT"
	echo "============================================================"

	if [ -x "$OUTPUT/bin/${TARGET}-gcc" ]; then
		echo "Toolchain for $ARCH already exists at $OUTPUT/bin/${TARGET}-gcc. Skipping."
		return 0
	fi

	# 1. Setup musl-cross-make repository
	mkdir -p "$DL_DIR" "$TOOLCHAIN_DIR" "$(dirname "$MCM_DIR")"
	if [ ! -d "$MCM_DIR/.git" ]; then
		echo "Cloning musl-cross-make..."
		git clone "$MCM_REPO" "$MCM_DIR"
	fi
	(
		cd "$MCM_DIR"
		CURRENT_REV=$(git rev-parse HEAD 2>/dev/null || echo "")
		if [ "$CURRENT_REV" != "$MCM_COMMIT" ]; then
			echo "Checking out musl-cross-make commit $MCM_COMMIT..."
			git fetch origin
			git checkout "$MCM_COMMIT"
		fi
	)

	# Ensure sources directory is cached across builds
	if [ ! -L "$MCM_DIR/sources" ] && [ ! -d "$MCM_DIR/sources" ]; then
		ln -sfn "$DL_DIR" "$MCM_DIR/sources"
	fi

	# 2. Inject patches
	mkdir -p "$MCM_DIR/patches/musl-$MUSL_VERSION"
	# Remove accidental upstream subdirectory in patches that breaks `cat patches/*`
	rm -rf "$MCM_DIR/patches/musl-$MUSL_VERSION/t" 2>/dev/null || true
	# Clean up any stale partial unpacking directory
	rm -rf "$MCM_DIR"/*.orig.tmp "$MCM_DIR"/*.tmp 2>/dev/null || true

	if [ -d "$PATCH_DIR" ]; then
		echo "Copying patches from $PATCH_DIR to musl-cross-make..."
		cp -f "$PATCH_DIR"/*.patch "$MCM_DIR/patches/musl-$MUSL_VERSION/" 2>/dev/null || true
	fi

	# 3. Generate config.mak
	local CONFIG_FILE="$MCM_DIR/config.mak"
	cat > "$CONFIG_FILE" <<EOF
TARGET = $TARGET
OUTPUT = $OUTPUT

GCC_VER = $GCC_VERSION
BINUTILS_VER = $BINUTILS_VERSION
MUSL_VER = $MUSL_VERSION

COMMON_CONFIG += CFLAGS="-g0 -O2" CXXFLAGS="-g0 -O2" LDFLAGS="-s"
COMMON_CONFIG += --disable-nls
GCC_CONFIG += --enable-languages=c,c++ --disable-libquadmath --disable-decimal-float --disable-libitm

EOF

	# Add Debian baseline architecture flags
	case "$ARCH" in
		armhf)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=armv7-a --with-fpu=vfpv3-d16 --with-float=hard --with-mode=thumb
EOF
			;;
		armel)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=armv5te --with-float=soft
EOF
			;;
		ppc6xx)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-cpu=powerpc
EOF
			;;
		riscv64)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=rv64gc --with-abi=lp64d
EOF
			;;
		i686)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=i686 --with-tune=generic
EOF
			;;
		x86_64)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=x86-64 --with-tune=generic
EOF
			;;
		aarch64)
			cat >> "$CONFIG_FILE" <<EOF
COMMON_CONFIG += --with-arch=armv8-a
EOF
			;;
	esac

	# 4. Download and extract all source dependencies first to avoid parallel make race
	echo "Extracting and patching sources..."
	make -C "$MCM_DIR" -j"$JOBS" extract_all

	# Ensure linux-headers has riscv symlinks (missing in linux-headers-4.19.88-2 arch/)
	for hdr_dir in "$MCM_DIR"/linux-headers-*; do
		if [ -d "$hdr_dir/arch" ] && [ ! -e "$hdr_dir/arch/riscv" ] && [ -d "$hdr_dir/riscv64" ]; then
			(cd "$hdr_dir/arch" && ln -s ../riscv64 riscv && ln -s ../riscv64 riscv64) 2>/dev/null || true
		fi
	done

	# 5. Build and install toolchain
	echo "Building toolchain (jobs: $JOBS)..."
	make -C "$MCM_DIR" -j"$JOBS" install

	# 6. Clean up intermediate build objects to save disk space while keeping extracted sources
	rm -rf "$MCM_DIR/build/local/$TARGET" || true

	echo "Musl toolchain successfully built and installed to: $OUTPUT"
}

# Package an already-installed toolchain into build/linux-musl-toolchain-<arch>.tar.gz
package_arch() {
	local arch="$1"
	local toolchain_dir="$TOOLCHAIN_DIR/$arch"
	local dist_dir="$NZBGET_ROOT/build"
	local tarball_name="linux-musl-toolchain-${arch}.tar.gz"
	local tarball_path="$dist_dir/$tarball_name"
	local sha256_path="$tarball_path.sha256"

	if [ ! -d "$toolchain_dir" ] || [ ! -d "$toolchain_dir/bin" ]; then
		echo "ERROR: Toolchain for '$arch' not found at: $toolchain_dir" >&2
		echo "Please build it first: $0 $arch" >&2
		return 1
	fi

	mkdir -p "$dist_dir"

	echo "Packaging toolchain for '$arch' -> $tarball_name..."
	(
		cd "$TOOLCHAIN_DIR" && \
		tar --exclude='.DS_Store' -czf "$tarball_path" "$arch"
	)

	echo "Generating SHA-256 checksum..."
	(
		cd "$dist_dir" && \
		if command -v sha256sum >/dev/null 2>&1; then
			sha256sum "$tarball_name" > "$tarball_name.sha256"
		else
			shasum -a 256 "$tarball_name" > "$tarball_name.sha256"
		fi
	)

	echo "Packaged: $tarball_path"
	echo "Checksum: $(cat "$sha256_path")"
}

# Main entrypoint
main() {
	local PACKAGE_ONLY=0
	local WITH_PACKAGE=0
	local TARGET_ARCH=""

	while [ $# -gt 0 ]; do
		case "$1" in
			package)
				PACKAGE_ONLY=1
				shift
				;;
			--package|-p)
				WITH_PACKAGE=1
				shift
				;;
			-h|--help)
				echo "Usage: $0 [package] <x86_64|aarch64|armhf|armel|i686|ppc6xx|riscv64|all> [--package]"
				echo ""
				echo "Commands:"
				echo "  $0 <arch|all>             Build toolchain without packaging"
				echo "  $0 <arch|all> --package   Build toolchain and package into build/linux-musl-toolchain-<arch>.tar.gz"
				echo "  $0 package <arch|all>     Package existing toolchain from build/toolchains/linux/<arch>"
				exit 0
				;;
			*)
				if [ -z "$TARGET_ARCH" ]; then
					TARGET_ARCH="$1"
				else
					echo "Unknown argument: $1" >&2
					exit 1
				fi
				shift
				;;
		esac
	done

	if [ -z "$TARGET_ARCH" ]; then
		echo "Usage: $0 [package] <x86_64|aarch64|armhf|armel|i686|ppc6xx|riscv64|all> [--package]"
		exit 1
	fi

	local arch_list="$TARGET_ARCH"
	if [ "$TARGET_ARCH" = "all" ]; then
		arch_list="$ALL_ARCHS"
	fi

	if [ "$PACKAGE_ONLY" -eq 1 ]; then
		for arch in $arch_list; do
			package_arch "$arch"
		done
		exit 0
	fi

	for arch in $arch_list; do
		build_arch "$arch"
		if [ "$WITH_PACKAGE" -eq 1 ]; then
			package_arch "$arch"
		fi
	done
}

main "$@"
exit 0
