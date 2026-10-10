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

The async engine uses `epoll` on Linux by default. Optionally, `-DENABLE_IO_URING=ON` switches it to `io_uring`, which requires **Linux 5.10+** and the `liburing` development package:

- **Debian / Ubuntu / Mint**: `sudo apt-get install -y liburing-dev`
- **Fedora / RHEL / CentOS Stream**: `sudo dnf install -y liburing-devel`
- **Arch Linux / Manjaro**: `sudo pacman -S --needed liburing`

> **Note on io_uring**: the option is Linux-only and is `OFF` by default. `liburing` is always taken from the system, even with `BUILD_DEPS_FROM_SOURCE=ON`; it is not downloaded by FetchContent. When enabled, `epoll` support is compiled out, so the resulting binary requires a kernel where the `io_uring` syscalls are available and not blocked (for example by a container's seccomp profile). Leave it `OFF` if you ship one binary for a range of kernels or run inside restricted containers.

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
| `ENABLE_IO_URING` | `OFF` | Linux only: use `io_uring` as the default async I/O reactor instead of `epoll` (requires `liburing` and Linux 5.10+) |
| `DISABLE_CURSES` | `OFF` | Disable the curses terminal UI (headless servers / containers) |
| `DISABLE_GZIP` | `OFF` | Disable gzip compression (zlib) |
| `DISABLE_PARCHECK` | `OFF` | Disable built-in par2 verification and repair |

`ENABLE_IO_URING` can be enabled on top of any preset without a dedicated one:

```bash
cmake --preset release -DENABLE_IO_URING=ON
```

## 5. Cross-Compilation with Musl Toolchains

NZBGet cross-compiles fully static binaries with **GCC 14.3.0 + Musl 1.2.6 + Binutils 2.44** toolchains built via `musl-cross-make`.

### 5.1 Kernel Time64 Compatibility (NAS and Legacy Kernels)

Musl 1.2+ uses 64-bit `time_t` on 32-bit platforms. On older Linux kernels (2.6.x, 3.x, 4.x) of older NAS devices (Synology, QNAP, D-Link, Netgear) the 64-bit time syscalls (`clock_gettime64`, `futex_time64`, `ppoll_time64`, etc.) return `-EINVAL` or fail. NZBGet ships `toolchains/linux/patches/musl-1.2.6/0010-time64-kernel-compat.patch`, which falls back to the 32-bit time syscalls on `-EINVAL` and caches the kernel capabilities in an atomic `__has_time64` flag, preventing recurring failed syscalls.

### 5.2 Building and Packaging Toolchains with Docker

The Docker builder is based on `debian:trixie-slim`, so the cross-compilers can be built on macOS (Intel or Apple Silicon), Windows WSL2 or Linux without installing anything on the host. All examples in this section and in the following ones use Docker; on a Linux host the same commands run directly, without the `docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder` prefix.

> **macOS Apple Silicon (M1/M2/M3/M4) / ARM hosts:** GitHub Actions runners (`ubuntu-24.04`) are **x86_64**. Pass `--platform linux/amd64` to `docker build` and `docker run`, so that the cross-compiler binaries are built for `x86_64` Linux hosts.

Build the Docker image:

```bash
# On x86_64 hosts (or native builds):
docker build -t nzbget-musl-builder toolchains/linux

# On non x86_64 hosts when building x86_64 toolchains for CI runners:
docker build --platform linux/amd64 -t nzbget-musl-builder toolchains/linux
```

Build toolchains into `build/toolchains/linux/<arch>`:

```bash
# A single architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh aarch64

# All 7 architectures sequentially
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh all
```

Build and package in one step (`--package`), creating compressed tarballs and SHA-256 checksums in `build/`:

```bash
# A single architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh armhf --package

# All 7 architectures
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh all --package
```

Package already built toolchains without rebuilding:

```bash
# A single architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh package armhf

# All 7 architectures
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh package all
```

Prerequisites for running the script on a host (Debian / Ubuntu):

```bash
sudo apt-get update && sudo apt-get install -y build-essential bison flex texinfo wget git
```

The resulting archives are placed in `build/`:

- `build/linux-musl-toolchain-<arch>.tar.gz`
- `build/linux-musl-toolchain-<arch>.tar.gz.sha256`

### 5.3 Manual Toolchain Setup with musl-cross-make

1. Clone musl-cross-make:
   ```bash
   git clone https://github.com/richfelker/musl-cross-make.git
   cd musl-cross-make
   git checkout fe915821b652a7fa37b34a596f47d8e20bc72338
   ```

2. Copy the time64 kernel compatibility patch:
   ```bash
   mkdir -p patches/musl-1.2.6
   cp /path/to/nzbget/toolchains/linux/patches/musl-1.2.6/0010-time64-kernel-compat.patch patches/musl-1.2.6/
   ```

3. Create `config.mak`:
   ```makefile
   TARGET = x86_64-linux-musl
   OUTPUT = /path/to/nzbget/build/toolchains/linux/x86_64
   GCC_VER = 14.3.0
   BINUTILS_VER = 2.44
   MUSL_VER = 1.2.6
   COMMON_CONFIG += CFLAGS="-g0 -O2" CXXFLAGS="-g0 -O2" LDFLAGS="-s"
   ```

   CPU baselines for the architectures where they are needed (prevents `SIGILL` on older devices):
   - **armhf**: `COMMON_CONFIG += --with-arch=armv7-a --with-fpu=vfpv3-d16 --with-float=hard --with-mode=thumb`
   - **armel**: `COMMON_CONFIG += --with-arch=armv5te --with-float=soft`
   - **ppc6xx**: `COMMON_CONFIG += --with-cpu=powerpc`
   - **riscv64**: `COMMON_CONFIG += --with-arch=rv64gc --with-abi=lp64d`
   - **i686**: `COMMON_CONFIG += --with-arch=i686 --with-tune=generic`

4. Compile and install:
   ```bash
   make
   make install
   ```

### 5.4 Building NZBGet with CMake Presets

With the toolchains installed in `build/toolchains/linux/<arch>` (or in a custom path pointed to by `NZBGET_LINUX_TOOLCHAIN_DIR`), NZBGet is built with the presets from `CMakePresets.json`:

| Architecture | Release Preset | Debug Preset |
|---|---|---|
| `x86_64` | `cmake --preset ci-linux-x86_64` | `cmake --preset ci-linux-debug-x86_64` |
| `aarch64` | `cmake --preset ci-linux-aarch64` | `cmake --preset ci-linux-debug-aarch64` |
| `armhf` | `cmake --preset ci-linux-armhf` | `cmake --preset ci-linux-debug-armhf` |
| `armel` | `cmake --preset ci-linux-armel` | `cmake --preset ci-linux-debug-armel` |
| `i686` | `cmake --preset ci-linux-i686` | `cmake --preset ci-linux-debug-i686` |
| `ppc6xx` | `cmake --preset ci-linux-ppc6xx` | `cmake --preset ci-linux-debug-ppc6xx` |
| `riscv64` | `cmake --preset ci-linux-riscv64` | `cmake --preset ci-linux-debug-riscv64` |

With Docker (example: aarch64 release):

```bash
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder cmake --preset ci-linux-aarch64
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder cmake --build --preset ci-linux-aarch64
```

## 6. Automated Packaging (`platforms/linux/build.sh`)

`platforms/linux/build.sh` automates the build and packaging lifecycle and writes the archives to `build/dist/`.

### 6.1 Packaging Targets

```bash
# Binary package for one architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh bin armhf release

# Binary packages for all 7 architectures
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh bin all release

# Universal multi-architecture installer
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh installer all release

# Everything at once: all 7 architectures, .tar.gz packages and the .run installer
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh all all release
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
