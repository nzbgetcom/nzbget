# Building NZBGet for FreeBSD Platforms

This document provides a comprehensive guide for configuring, building, testing, and packaging NZBGet for FreeBSD operating systems, covering **FreeBSD 13.0-RELEASE** and newer across both **x86_64 (amd64)** and **aarch64 (arm64)** architectures.

NZBGet supports two build workflows:
1. **Native Build on FreeBSD**: Compile directly on an existing FreeBSD system using native tools and packages.
2. **Cross-Compilation with LLVM Clang 19**: Compile FreeBSD binaries from Linux or macOS using a hermetic FreeBSD base sysroot and LLVM 19 toolchains.

---

## 1. Supported Architectures

| Architecture | FreeBSD Arch | Target Triple | Typical Hardware |
|---|---|---|---|
| **x86_64** | `amd64` | `x86_64-pc-freebsd13` | Standard PC, Intel/AMD servers, FreeNAS / TrueNAS CORE VMs |
| **aarch64** | `arm64` | `aarch64-pc-freebsd13` | Raspberry Pi 4/5, Ampere Altra servers, AWS Graviton instances, Apple Silicon VMs |

---

## 2. Native Build on FreeBSD

When building directly on a FreeBSD machine, the base operating system provides modern Clang and fundamental C/C++ runtime libraries.

### 2.1 Dependencies

The FreeBSD base system already includes:
- **LLVM Clang / Clang++** (`/usr/bin/clang`, `/usr/bin/clang++`)
- **LLVM libc++ / libc++abi** (`/usr/lib/libc++.so`)
- **OpenSSL** (`/usr/lib/libssl.so`, `/usr/lib/libcrypto.so`)
- **zlib** (`/usr/lib/libz.so`)
- **ncursesw** (`/usr/lib/libncursesw.so`, `/usr/include/ncurses.h`)

Install the required build tools and external libraries using `pkg`:

```bash
pkg install -y cmake ninja git pkgconf textproc/libxml2
```

### 2.2 Third-Party Dependencies Resolution

NZBGet manages dependencies based on the CMake cache variable `BUILD_DEPS_FROM_SOURCE`:
- **Boost**: Header-only integration via CMake FetchContent (`Boost.JSON`, `Boost.Asio`, `Boost.Beast`), providing modern networking and JSON parsing without requiring precompiled Boost packages.
- **rapidyenc**: High-performance SIMD-accelerated yEnc decoding engine, always compiled from source via CMake FetchContent.
- **par2-turbo**: High-performance PAR2 verification and repair engine, always compiled from source via CMake FetchContent.
- **OpenSSL, zlib, libxml2**: In native dev builds (`BUILD_DEPS_FROM_SOURCE=OFF`), detected automatically from the FreeBSD base system or `pkg`. In hermetic CI builds (`BUILD_DEPS_FROM_SOURCE=ON`), compiled from source as isolated static libraries.
- **ncursesw**: Wide-character curses library (`libncursesw.a` in static CI builds, `libncursesw.so` in native dev builds).

### 2.3 Building with CMake Presets

NZBGet provides standard native CMake presets in `CMakePresets.json`:

```bash
# Clone the repository
git clone https://github.com/nzbgetcom/nzbget.git
cd nzbget

# Option A: Standard Optimized Release build
cmake --preset release
cmake --build --preset release -j$(sysctl -n hw.ncpu)

# Option B: Release build with Link-Time Optimization (LTO)
cmake --preset release-lto
cmake --build --preset release-lto -j$(sysctl -n hw.ncpu)

# Option C: Debug build (for development and debugging)
cmake --preset debug
cmake --build --preset debug -j$(sysctl -n hw.ncpu)

# Option D: Standalone / Hermetic Build (compiles dependencies from source)
cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS_FROM_SOURCE=ON
cmake --build build/release -j$(sysctl -n hw.ncpu)
```

The resulting executable is located at:
```bash
./build/release/nzbget -v
```

To run NZBGet in server or console mode:
```bash
# Run in standalone console mode
./build/release/nzbget -s -c nzbget.conf

# Run in background server mode
./build/release/nzbget -D -c nzbget.conf
```

---

## 3. Cross-Compilation (from Linux / macOS Host)

Cross-compilation allows developers and CI/CD pipelines to build FreeBSD binaries on Linux (e.g. Ubuntu 22.04/24.04, Debian) or macOS without needing a running FreeBSD host.

### 3.1 Host Prerequisites

Install LLVM Clang 19, LLD, CMake, and archive utilities:

```bash
# Ubuntu 24.04 / Debian
sudo apt-get update
sudo apt-get install -y clang-19 lld-19 llvm-19 xz-utils curl cmake ninja-build git
```

### 3.2 Prepare FreeBSD Sysroots (`toolchains/build-freebsd.sh`)

NZBGet cross-compiles against official FreeBSD base distributions (`base.txz`). The helper script downloads the distribution from official FreeBSD mirrors, extracts headers and libraries, fixes absolute symlinks (`/lib/...` -> sysroot-relative), and links standard C++ runtime headers:

```bash
# Prepare x86_64 sysroot (build/toolchains/freebsd/sysroot)
bash toolchains/build-freebsd.sh amd64 13.0

# Prepare aarch64 sysroot (build/toolchains/freebsd/sysroot-aarch64)
bash toolchains/build-freebsd.sh arm64 13.0
```

Sysroot locations:
- `build/toolchains/freebsd/sysroot` (`x86_64` / `amd64`)
- `build/toolchains/freebsd/sysroot-aarch64` (`aarch64` / `arm64`)

### 3.3 Building with Cross-Compilation CMake Presets

NZBGet provides dedicated CI presets in `CMakePresets.json` that configure the toolchain (`toolchains/freebsd-llvm.cmake`), sysroot paths, target triples, and compile flags:

```bash
# Build x86_64 Release binary
cmake --preset ci-freebsd-x86_64
cmake --build --preset ci-freebsd-x86_64 -j$(nproc)

# Build aarch64 Release binary
cmake --preset ci-freebsd-aarch64
cmake --build --preset ci-freebsd-aarch64 -j$(nproc)

# Build Debug variants (includes debug symbols, no LTO)
cmake --preset ci-freebsd-debug-x86_64
cmake --build --preset ci-freebsd-debug-x86_64 -j$(nproc)

cmake --preset ci-freebsd-debug-aarch64
cmake --build --preset ci-freebsd-debug-aarch64 -j$(nproc)
```

The compiled binary will be placed at:
- `build/ci-freebsd-x86_64/nzbget`
- `build/ci-freebsd-aarch64/nzbget`

---

## 4. Packaging with `platforms/freebsd/build.sh`

The runner script automates sysroot checking, compilation, and assembly of official distribution artifacts:
- Per-architecture `.tar.gz` binary archives: `nzbget-<version>-bin-freebsd-<arch>.tar.gz`
- Unified multi-architecture self-extracting `.run` installer: `nzbget-<version>-bin-freebsd.run`

### 4.1 Build Commands

```bash
# 1. Build binary archives for a single architecture
bash platforms/freebsd/build.sh bin x86_64 release
bash platforms/freebsd/build.sh bin aarch64 release

# 2. Build binary archives for all architectures (Release)
bash platforms/freebsd/build.sh bin all-arch release

# 3. Assemble unified multi-arch self-extracting .run installer (combines x86_64 and aarch64)
bash platforms/freebsd/build.sh installer all-arch release

# 4. Build Debug installer (for testing on develop)
bash platforms/freebsd/build.sh bin all-arch debug
bash platforms/freebsd/build.sh installer all-arch debug
```

All distribution files are stored in `build/dist/`:
- `build/dist/nzbget-<version>-bin-freebsd-x86_64.tar.gz`
- `build/dist/nzbget-<version>-bin-freebsd-aarch64.tar.gz`
- `build/dist/nzbget-<version>-bin-freebsd.run`

---

## 5. CMake Presets Reference

| Preset Name | Target Architecture | Build Type | Linkage | Toolchain File | Target Triple |
|---|---|---|---|---|---|
| `release` | Host (native) | Release | Dynamic (host syslibs) | Native Host | Native |
| `release-lto` | Host (native) | Release + LTO | Dynamic (host syslibs) | Native Host | Native |
| `debug` | Host (native) | Debug | Dynamic (host syslibs) | Native Host | Native |
| `ci-freebsd-x86_64` | `x86_64` (amd64) | Release | **Static** | `toolchains/freebsd-llvm.cmake` | `x86_64-pc-freebsd13` |
| `ci-freebsd-aarch64` | `aarch64` (arm64) | Release | **Static** | `toolchains/freebsd-llvm.cmake` | `aarch64-pc-freebsd13` |
| `ci-freebsd-debug-x86_64` | `x86_64` (amd64) | Debug | **Static** | `toolchains/freebsd-llvm.cmake` | `x86_64-pc-freebsd13` |
| `ci-freebsd-debug-aarch64` | `aarch64` (arm64) | Debug | **Static** | `toolchains/freebsd-llvm.cmake` | `aarch64-pc-freebsd13` |

> **Note**: CI presets (`ci-freebsd-*`) are configured for hermetic, fully static linkage (`ENABLE_STATIC=ON`, `-static`) so that distribution `.run` packages run out-of-the-box on any FreeBSD system without requiring host development libraries or matching shared library versions. For local development directly on FreeBSD, native presets (`release`, `debug`) use the host system libraries dynamically for fastest compile/link cycles.

---

## 6. Deployment on FreeBSD

### 6.1 Installing the Self-Extracting `.run` Package

Run the self-extracting script on the target FreeBSD machine:

```bash
# Install to default directory (/usr/local/share/nzbget or ~/nzbget)
sh nzbget-25.0-bin-freebsd.run --destdir /usr/local/nzbget
```

### 6.2 FreeBSD `rc.d` Service Script

To manage NZBGet as a standard FreeBSD daemon service, create `/usr/local/etc/rc.d/nzbget`:

```sh
#!/bin/sh
#
# PROVIDE: nzbget
# REQUIRE: DAEMON NETWORKING
# KEYWORD: shutdown
#
# Add the following lines to /etc/rc.conf to enable nzbget:
# nzbget_enable="YES"
# nzbget_user="nzbget" (optional, default: nzbget)
# nzbget_conf="/usr/local/nzbget/nzbget.conf" (optional)

. /etc/rc.subr

name="nzbget"
rcvar="nzbget_enable"

load_rc_config $name

: ${nzbget_enable:="NO"}
: ${nzbget_user:="nzbget"}
: ${nzbget_dir:="/usr/local/nzbget"}
: ${nzbget_conf:="${nzbget_dir}/nzbget.conf"}

command="${nzbget_dir}/nzbget"
command_args="-D -c ${nzbget_conf}"
stop_cmd="${command} -Q -c ${nzbget_conf}"
status_cmd="${command} -L -c ${nzbget_conf}"

run_rc_command "$1"
```

Set permissions and enable the service:
```bash
chmod 755 /usr/local/etc/rc.d/nzbget
sysrc nzbget_enable="YES"

# Start the service
service nzbget start

# Check status
service nzbget status
```

---

## 7. Unpackers (UnRAR & 7-Zip)

NZBGet utilizes external unpackers to extract RAR and 7z archives:
- **x86_64 (amd64)** and **aarch64 (arm64)**: Pre-compiled static unpackers (`unrar`, `7za`) are automatically bundled inside the `.run` installer package.
- If you prefer system unpackers, install them via `pkg`:
  ```bash
  pkg install -y unrar 7-zip
  ```
- In `nzbget.conf`, configure the unpacker commands:
  ```ini
  UnrarCmd=${AppDir}/unrar
  SevenZipCmd=${AppDir}/7za
  ```
  Or point to system paths:
  ```ini
  UnrarCmd=/usr/local/bin/unrar
  SevenZipCmd=/usr/local/bin/7z
  ```

---

## 8. Troubleshooting

### 1. `FreeBSD sysroot not found`
**Error**: `CMake Error: FreeBSD sysroot not found in ...`
**Solution**: Run `bash toolchains/build-freebsd.sh <amd64|arm64> 13.0` before configuring the preset.

### 2. Missing Curses / Terminal Support
**Error**: Terminal displays garbage characters or curses initialization fails.
**Solution**: Ensure `TERM` environment variable is set (`export TERM=xterm-256color`) and `/usr/share/misc/termcap` exists on FreeBSD.

### 3. Missing `libxml2` during Native Build
**Error**: `Could not find a package configuration file provided by "LibXml2"`
**Solution**: Install `pkg install -y textproc/libxml2` or force pure source compilation by adding `-DBUILD_DEPS_FROM_SOURCE=ON`.
