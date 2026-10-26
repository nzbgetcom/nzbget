# Building NZBGet for Linux Platforms

This document describes how to configure, build, test and package NZBGet for Linux. External dependencies are fetched and built from source with **CMake** (3.25+) and **FetchContent**.

## 1. Supported Architectures

NZBGet is built statically against musl libc for 7 architectures, using toolchains built with GCC 14.3.0 and musl 1.2.6:

| Architecture | Musl Triplet | Family / Baseline |
|---|---|---|
| **x86_64** | `x86_64-linux-musl` | x86 64-bit AMD/Intel baseline |
| **aarch64** | `aarch64-linux-musl` | ARM 64-bit (ARMv8-A+) |
| **armhf** | `arm-linux-musleabihf` | ARM 32-bit Hard Float (ARMv7-A, VFPv3-D16, Thumb-2) |
| **armel** | `arm-linux-musleabi` | ARM 32-bit Soft Float (ARMv5TE) |
| **i686** | `i686-linux-musl` | x86 32-bit Pentium Pro (generic, no mandatory SSE2) |
| **ppc6xx** | `powerpc-linux-musl` | PowerPC 32-bit (generic e300/603e, soft-float/classic FPU) |
| **riscv64** | `riscv64-linux-musl` | RISC-V 64-bit (RV64GC, lp64d ABI) |

## 2. Native Build on Linux

NZBGet requires **C++20** (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`) and **C17** (`CMAKE_C_STANDARD 17`, `CMAKE_C_STANDARD_REQUIRED ON`, `CMAKE_C_EXTENSIONS OFF`), a GCC 14+ or Clang 19+ compiler, CMake 3.25+ and Ninja (or GNU Make).

### 2.1 Prerequisites

**Core build tools:**

- **Debian / Ubuntu / Mint**:
  ```bash
  sudo apt-get update && sudo apt-get install -y build-essential cmake ninja-build git
  ```
- **Fedora / RHEL / CentOS Stream**:
  ```bash
  sudo dnf install -y gcc-c++ cmake ninja-build git
  ```
- **Arch Linux / Manjaro**:
  ```bash
  sudo pacman -S --needed base-devel cmake ninja git
  ```

**System libraries** (for Strategy 1):

- **Debian / Ubuntu / Mint**:
  ```bash
  sudo apt-get install -y libssl-dev libxml2-dev zlib1g-dev libncurses-dev libboost-dev libboost-json-dev
  ```
- **Fedora / RHEL / CentOS Stream**:
  ```bash
  sudo dnf install -y openssl-devel libxml2-devel zlib-devel ncurses-devel boost-devel
  ```
- **Arch Linux / Manjaro**:
  ```bash
  sudo pacman -S --needed openssl libxml2 zlib ncurses boost
  ```

> **Note on Boost**: NZBGet uses Boost.JSON for JSON parsing and serialization. If system Boost is missing or lacks Boost.JSON, CMake fetches Boost.JSON header-only via FetchContent.

NZBGet is built with curses terminal dashboard support (`nzbget -s`) by default. For headless environments (containers, NAS) that only need the WebUI and RPC daemon mode, configure with `-DDISABLE_CURSES=ON`; the `ncurses` development packages are then not required.

## 3. Dependency Management: Source vs System

The CMake cache variable `BUILD_DEPS_FROM_SOURCE` selects between two dependency resolution models:

| Model / Option | Value | Behavior | Recommended for |
|---|---|---|---|
| **Strategy 1: System Packages** (`BUILD_DEPS_FROM_SOURCE=OFF`) | `OFF` (default in dev presets) | Discovers and links pre-installed system libraries (`OpenSSL`, `zlib`, `libxml2`, `Boost`, `ncurses`); if a library is missing, CMake falls back to FetchContent. `rapidyenc` and `par2-turbo` are always built from source. | Local development, rapid compile times, distro packaging |
| **Strategy 2: Hermetic Release** (`BUILD_DEPS_FROM_SOURCE=ON`) | `ON` (default in `ci-*` presets) | Reproducible build: fetches and compiles all dependencies from source via CMake FetchContent, independent of host system libraries. | Standalone binaries, CI/CD, portable static `.run` installers |

**Dependencies managed via FetchContent:**

- **OpenSSL**: 3.x static library built from source via `jimmy-park/openssl-cmake`.
- **zlib**: static library built from `madler/zlib`.
- **libxml2**: static library built from GNOME GitLab (tests and Python bindings disabled).
- **Boost**: header-only integration of Boost.JSON (via `boostorg/boost`).
- **par2-turbo**: PAR2 verification and repair (static library).
- **rapidyenc**: SIMD-accelerated yEnc decoding (static library).

## 4. Build Commands (Native Linux)

### Strategy 1: System Packages

Standard Makefile:

```bash
cmake -B build -DBUILD_DEPS_FROM_SOURCE=OFF
cmake --build build
```

Ninja:

```bash
cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=OFF
cmake --build build
```

Presets from `CMakePresets.json`:

```bash
cmake --preset release
cmake --build --preset release
```

Debug build with unit tests:

```bash
cmake --preset debug-tests
cmake --build --preset debug-tests
ctest --preset debug-tests --output-on-failure
```

Headless build (WebUI / daemon only, no curses):

```bash
cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=OFF -DDISABLE_CURSES=ON
cmake --build build
```

### Strategy 2: Hermetic Release

```bash
cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=ON -DENABLE_STATIC=ON
cmake --build build
```

The executable is located at `build/nzbget` (or `build/release/nzbget`).

Running:

```bash
# Interactive console dashboard mode
./build/nzbget -s -c nzbget.conf

# Background daemon server mode
./build/nzbget -D -c nzbget.conf
```

### 4.1 Installation and System Integration

Installing into a specific prefix (default is `/usr/local`):

```bash
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/opt/nzbget
cmake --build build
cmake --install build
```

Configuration files:

```bash
# Install default nzbget.conf to <prefix>/etc/nzbget.conf
cmake --build build --target install-conf

# Uninstall configuration files
cmake --build build --target uninstall-conf
```

Uninstalling:

```bash
cmake --build build --target uninstall
```

### 4.2 CMake Configuration Options

| Option | Default | Description |
|---|---|---|
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (dev) / `ON` (CI) | When `ON`, downloads and builds all dependencies via CMake FetchContent |
| `ENABLE_STATIC` | `OFF` | Build a 100% statically linked executable (`-static`) |
| `ENABLE_TESTS` | `OFF` | Build unit test suites (`tests/`) |
| `ENABLE_CLANG_TIDY` | `OFF` | Enable the Clang-Tidy static code analyzer during compilation |
| `DISABLE_CURSES` | `OFF` | Disable the curses terminal UI (headless servers / containers) |
| `DISABLE_GZIP` | `OFF` | Disable gzip compression (zlib) |
| `DISABLE_PARCHECK` | `OFF` | Disable built-in par2 verification and repair |

## 5. Cross-Compilation with Musl Toolchains

NZBGet cross-compiles fully static binaries with **GCC 14.3.0 + Musl 1.2.6 + Binutils 2.44**. Those compilers are published as images from [nzbgetcom/nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder). The tag suffix is the `VERSION` file in that repository (currently `1.0`). Images are `linux/amd64`; on Apple Silicon pass `--platform linux/amd64`.

| Architecture | Image |
|---|---|
| `x86_64` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-x86_64-1.0` |
| `aarch64` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-aarch64-1.0` |
| `armhf` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-armhf-1.0` |
| `armel` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-armel-1.0` |
| `i686` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-i686-1.0` |
| `ppc6xx` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-ppc6xx-1.0` |
| `riscv64` | `ghcr.io/nzbgetcom/nzbget-musl-builder:linux-riscv64-1.0` |
| every Linux arch, or a multi-arch installer | `ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0` |

Mount the NZBGet sources on `/src`. The image sets `NZBGET_LINUX_TOOLCHAIN_DIR=/opt/nzbget/linux`.

### 5.1 Kernel Time64 Compatibility (NAS and Legacy Kernels)

Musl 1.2+ uses 64-bit `time_t` on 32-bit platforms. On older Linux kernels (2.6.x, 3.x, 4.x) of older NAS devices (Synology, QNAP, D-Link, Netgear) the 64-bit time syscalls (`clock_gettime64`, `futex_time64`, `ppoll_time64`, etc.) return `-EINVAL` or fail. The published toolchains include a musl patch that falls back to the 32-bit time syscalls on `-EINVAL` and caches the kernel capabilities in an atomic `__has_time64` flag, preventing recurring failed syscalls.

### 5.2 Building NZBGet with CMake Presets

NZBGet is built with the presets from `CMakePresets.json`:

| Architecture | Release Preset | Debug Preset |
|---|---|---|
| `x86_64` | `cmake --preset ci-linux-x86_64` | `cmake --preset ci-linux-debug-x86_64` |
| `aarch64` | `cmake --preset ci-linux-aarch64` | `cmake --preset ci-linux-debug-aarch64` |
| `armhf` | `cmake --preset ci-linux-armhf` | `cmake --preset ci-linux-debug-armhf` |
| `armel` | `cmake --preset ci-linux-armel` | `cmake --preset ci-linux-debug-armel` |
| `i686` | `cmake --preset ci-linux-i686` | `cmake --preset ci-linux-debug-i686` |
| `ppc6xx` | `cmake --preset ci-linux-ppc6xx` | `cmake --preset ci-linux-debug-ppc6xx` |
| `riscv64` | `cmake --preset ci-linux-riscv64` | `cmake --preset ci-linux-debug-riscv64` |

Example: aarch64 release. Use the image for the architecture you are building.

```bash
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:linux-aarch64-1.0 \
  cmake --preset ci-linux-aarch64
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:linux-aarch64-1.0 \
  cmake --build --preset ci-linux-aarch64
```

## 6. Automated Packaging (`platforms/linux/build.sh`)

`platforms/linux/build.sh` automates the build and packaging lifecycle and writes the archives to `build/dist/`.

### 6.1 Packaging Targets

```bash
# Binary package for one architecture
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:linux-armhf-1.0 \
  bash platforms/linux/build.sh bin armhf release

# Binary packages for all 7 architectures
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/linux/build.sh bin all release

# Universal multi-architecture installer
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/linux/build.sh installer all release

# Everything at once: all 7 architectures, .tar.gz packages and the .run installer
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/linux/build.sh all all release
```

- Binary packages (`.tar.gz`) contain NZBGet and the pre-built unpackers (`unrar` and `7za`): `build/dist/nzbget-<version>-bin-linux-<arch>.tar.gz`
- The universal installer (`.run`) is a single self-extracting archive with the binaries of all 7 architectures, the unpackers, the configuration files, the WebUI and the certificates: `build/dist/nzbget-<version>-bin-linux.run`

### 6.2 Linux Distribution Packages

- **Debian / Ubuntu (`.deb`)**:
  ```bash
  bash platforms/linux/pkg/build-pkg.sh deb
  ```
- **Fedora / RHEL / openSUSE (`.rpm`)**:
  ```bash
  bash platforms/linux/pkg/build-pkg.sh rpm
  ```
- **Flatpak**:
  ```bash
  bash platforms/linux/flatpak/build-flatpak.sh
  ```
- **Snap**:
  ```bash
  cd platforms/linux/snap && snapcraft --destructive-mode
  ```
- **QNAP QPKG (`.qpkg`)**:
  ```bash
  bash platforms/linux/qnap/repack-nzbget.sh <path_to_installer.run>
  ```

## 7. Security and Hardening Features

All Linux release binaries are statically linked against musl libc with the following compiler and linker hardening:

- **Full RELRO and BIND_NOW**: `-Wl,-z,relro,-z,now`
- **Non-executable stack**: `-Wl,-z,noexecstack`
- **Stack protection**: `-fstack-protector-strong` and `-fstack-clash-protection` (on supported targets)
- **Control-flow integrity**:
  - Intel/AMD (`x86_64`): `-fcf-protection=full` (Intel CET)
  - ARM 64-bit (`aarch64`): `-mbranch-protection=standard` (PAC and BTI)
- **Source fortification**: `-D_FORTIFY_SOURCE=2` or `=3` (enabled when building against a libc with `__*_chk` symbol support)
- **C++ standard library assertions**: `-D_GLIBCXX_ASSERTIONS` (GCC Debug / RelWithDebInfo builds)
- **Visibility**: `-fvisibility=hidden` and `-fvisibility-inlines-hidden`
- **RTTI disabled in release builds**: `-fno-rtti`
- **Linker section pruning**: `-Wl,--gc-sections` with `-ffunction-sections` / `-fdata-sections`
- **Link-time optimization (LTO)**: interprocedural optimization enabled across release builds
