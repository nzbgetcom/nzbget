# Building NZBGet for Linux Platforms

This document provides a comprehensive guide for configuring, building, testing, and packaging NZBGet for Linux operating systems across all 7 officially supported architectures: **x86_64**, **aarch64**, **armhf**, **armel**, **i686**, **ppc6xx**, and **riscv64**.

NZBGet uses modern **CMake** (3.25+) with **FetchContent** to manage external dependencies cleanly, deterministically, and hermetically.

---

## 1. Supported Architectures

NZBGet targets 7 CPU architectures for Linux with 100% static linkage via musl libc and GCC 14.3 (Bootlin toolchains):

| Architecture | Musl Triplet | Family / Baseline |
|---|---|---|
| **x86_64** | `x86_64-linux-musl` | x86 64-bit AMD/Intel baseline |
| **aarch64** | `aarch64-linux-musl` | ARM 64-bit (ARMv8-A+) |
| **armhf** | `arm-linux-musleabihf` | ARM 32-bit Hard Float (ARMv7-A, VFPv3-D16, Thumb-2) |
| **armel** | `arm-linux-musleabi` | ARM 32-bit Soft Float (ARMv5TE) |
| **i686** | `i686-linux-musl` | x86 32-bit Pentium Pro (generic, no mandatory SSE2) |
| **ppc6xx** | `powerpc-linux-musl` | PowerPC 32-bit (generic e300/603e, soft-float/classic FPU) |
| **riscv64** | `riscv64-linux-musl` | RISC-V 64-bit (RV64GC, lp64d ABI) |

---

## 2. Native Build on Linux

Building natively on Linux allows compiling NZBGet directly for the host system using GCC or Clang.

### 2.1 Compiler and Language Standard Requirements

NZBGet strictly requires **C++20** and **C17**:
- **C++ Standard**: C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`)
- **C Standard**: C17 (`CMAKE_C_STANDARD 17`, `CMAKE_C_STANDARD_REQUIRED ON`, `CMAKE_C_EXTENSIONS OFF`)
- **Compilers**: GCC 14+ or Clang 19+
- **Build System**: CMake 3.25+ and Ninja (or GNU Make)

### 2.2 Prerequisites

#### 1. Core Build Tools (Required)
Install a C++20 compiler, CMake, Git, and optionally Ninja:

- **Debian / Ubuntu / Mint**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build git
  ```
- **Fedora / RHEL / CentOS Stream**:
  ```bash
  sudo dnf install -y gcc-c++ cmake ninja-build git
  ```
- **Arch Linux / Manjaro**:
  ```bash
  sudo pacman -S --needed base-devel cmake ninja git
  ```

#### 2. System Libraries (Required for Strategy 1: Local Development)
To build dynamically against pre-installed distribution libraries without downloading source packages:

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

> **Note on Boost**: NZBGet utilizes Boost.JSON for JSON parsing and serialization. If system Boost is not installed or lacks Boost.JSON, CMake automatically fetches and integrates Boost.JSON header-only via FetchContent.

#### 3. Optional: Headless Mode (Disabling Curses)
By default, NZBGet builds with curses terminal dashboard support (`nzbget -s`).
If you are running in a headless environment (containers, NAS, headless servers) and only need WebUI and RPC daemon mode, you can disable curses by passing `-DDISABLE_CURSES=ON`. When building headless, `ncurses` development packages are not required.

---

## 3. Dependency Management: Source vs System

NZBGet supports two dependency resolution models controlled by the CMake cache variable `BUILD_DEPS_FROM_SOURCE`:

| Model / Option | Value | Behavior | Recommended For |
|---|---|---|---|
| **Strategy 1: System Packages** (`BUILD_DEPS_FROM_SOURCE=OFF`) | `OFF` (default in dev presets) | Discovers and links pre-installed system libraries (`OpenSSL`, `zlib`, `libxml2`, `Boost`, `ncurses`). If any library is missing, CMake transparently falls back to FetchContent. `rapidyenc` and `par2-turbo` are always built from source. | Local development, rapid compile times, distro packaging |
| **Strategy 2: Hermetic Release** (`BUILD_DEPS_FROM_SOURCE=ON`) | `ON` (default in `ci-*` presets) | Fully isolated, reproducible build: fetches and compiles all dependencies from source via CMake FetchContent, completely independent of host system libraries. | Standalone binaries, CI/CD, portable static `.run` installers |

### Dependencies Managed via FetchContent
- **OpenSSL**: 3.x static library built from source via `jimmy-park/openssl-cmake`.
- **zlib**: Static library built from `madler/zlib`.
- **libxml2**: Static library built from GNOME GitLab (tests and Python bindings disabled).
- **Boost**: Header-only integration of Boost.JSON (via `boostorg/boost`).
- **par2-turbo**: High-performance PAR2 verification and repair (static library).
- **rapidyenc**: High-performance SIMD-accelerated yEnc decoding (static library).

---

## 4. Build Strategies (Native Linux)

### Strategy 1: Fast Local Development (System Packages)
Best for day-to-day development. Uses pre-installed system packages dynamically for rapid compilation.

- **Default Build (Standard Makefile)**:
  ```bash
  cmake -B build -DBUILD_DEPS_FROM_SOURCE=OFF
  cmake --build build
  ```

- **With Ninja (if installed)**:
  ```bash
  cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=OFF
  cmake --build build
  ```

  Or using standard presets from `CMakePresets.json`:
  ```bash
  cmake --preset release
  cmake --build --preset release
  ```

- **Debug Build with Unit Tests**:
  ```bash
  cmake --preset debug-tests
  cmake --build --preset debug-tests
  ctest --preset debug-tests --output-on-failure
  ```

- **Headless Build (WebUI / Daemon Only, no curses)**:
  ```bash
  cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=OFF -DDISABLE_CURSES=ON
  cmake --build build
  ```

### Strategy 2: Standalone Hermetic Release Build (FetchContent + Static)
Downloads and compiles all dependencies from source, producing a completely self-contained, statically linked binary that runs on any compatible Linux distribution regardless of installed packages:

```bash
cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=ON -DENABLE_STATIC=ON
cmake --build build
```

The resulting executable is located at `build/nzbget` (or `build/release/nzbget`).

To run NZBGet:
```bash
# Interactive console dashboard mode
./build/nzbget -s -c nzbget.conf

# Background daemon server mode
./build/nzbget -D -c nzbget.conf
```

---

### 4.1 Installation & System Integration

NZBGet provides standard CMake install targets for system-wide or user-local installation:

- **Installing into a specific prefix** (default is `/usr/local`):
  ```bash
  cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/opt/nzbget
  cmake --build build
  cmake --install build
  ```

- **Configuration file targets**:
  ```bash
  # Install default nzbget.conf to <prefix>/etc/nzbget.conf
  cmake --build build --target install-conf

  # Uninstall configuration files
  cmake --build build --target uninstall-conf
  ```

- **Uninstalling NZBGet**:
  ```bash
  cmake --build build --target uninstall
  ```

---

### 4.2 CMake Configuration Options Reference

| Option | Default | Description |
|---|---|---|
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (dev) / `ON` (CI) | When `ON`, downloads and builds all dependencies via CMake FetchContent |
| `ENABLE_STATIC` | `OFF` | Build 100% statically linked executable (`-static`) |
| `ENABLE_TESTS` | `OFF` | Build unit test suites (`tests/`) |
| `ENABLE_CLANG_TIDY` | `OFF` | Enable Clang-Tidy static code analyzer during compilation |
| `DISABLE_CURSES` | `OFF` | Disable curses terminal UI (useful for headless servers / containers) |
| `DISABLE_TLS` | `OFF` | Disable TLS/OpenSSL support |
| `DISABLE_GZIP` | `OFF` | Disable gzip compression (zlib) |
| `DISABLE_PARCHECK` | `OFF` | Disable built-in par2 verification and repair |

---

## 5. Cross-Compilation with Musl Toolchains

NZBGet uses modern, hermetic **GCC 14.3.0 + Musl 1.2.6 + Binutils 2.44** toolchains built via `musl-cross-make` to cross-compile fully static binaries across all 7 supported architectures.

### 5.1 Kernel Time64 Compatibility (NAS & Legacy Kernels)
Musl 1.2+ uses 64-bit `time_t` on 32-bit platforms. On older Linux kernels (2.6.x, 3.x, 4.x) commonly found on older NAS devices (Synology, QNAP, D-Link, Netgear), 64-bit time syscalls (`clock_gettime64`, `futex_time64`, `ppoll_time64`, etc.) return `-EINVAL` or fail.
NZBGet includes `toolchains/linux/patches/musl-1.2.6/0010-time64-kernel-compat.patch`, which transparently falls back to 32-bit time syscalls on `-EINVAL` and caches kernel capabilities via an atomic `__has_time64` flag, preventing recurring failed syscalls.

### 5.2 Building and Packaging Toolchains with Docker

To build cross-compilers reliably on any operating system (macOS with Apple Silicon or Intel, Windows WSL2, Linux) without polluting host packages, NZBGet provides a hermetic Docker builder based on `debian:trixie-slim`.

> **Important for macOS Apple Silicon (M1/M2/M3/M4) / ARM hosts:**
> GitHub Actions runners (`ubuntu-24.04`) execute on **x86_64 host CPU architecture**.
> When preparing toolchains to be uploaded for CI or GitHub releases, pass `--platform linux/amd64` to `docker build` and `docker run` so the cross-compiler binaries themselves are built for `x86_64` Linux hosts!

#### 1. Build the Docker Image
```bash
# On x86_64 hosts (or native builds):
docker build -t nzbget-musl-builder toolchains/linux

# On non x86_64 hosts when building x86_64 toolchains for CI runners:
docker build --platform linux/amd64 -t nzbget-musl-builder toolchains/linux
```

#### 2. Build Toolchains
```bash
# Build a specific architecture into build/toolchains/linux/<arch>
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh x86_64
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh aarch64

# Or build all 7 architectures sequentially
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh all

# Note: On non x86_64 hosts for CI releases, add --platform linux/amd64:
docker run --platform linux/amd64 --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh all
```

#### 3. Build and Package Automatically (`--package`)
To compile and immediately generate compressed tarballs and SHA-256 checksums in `build/`:
```bash
# Build and package a single architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh armhf --package

# Build and package all 7 architectures
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh all --package
```

#### 4. Package Already Built Toolchains (`package`)
If toolchains are already built in `build/toolchains/linux/<arch>`, package them without rebuilding:
```bash
# Package a specific architecture
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh package armhf

# Package all 7 architectures
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash toolchains/linux/build-toolchain.sh package all
```

The resulting archives and checksums are placed directly in `build/`:
- `build/linux-musl-toolchain-<arch>.tar.gz`
- `build/linux-musl-toolchain-<arch>.tar.gz.sha256`

---

### 5.3 Building Toolchains Natively on Host with `toolchains/linux/build-toolchain.sh`

On a Linux host machine, the helper script `toolchains/linux/build-toolchain.sh` can also be run directly:

```bash
# Prerequisites for building toolchains (Debian/Ubuntu)
sudo apt-get update && sudo apt-get install -y build-essential bison flex texinfo wget git

# Build toolchain for a specific target architecture
bash toolchains/linux/build-toolchain.sh x86_64
bash toolchains/linux/build-toolchain.sh aarch64
bash toolchains/linux/build-toolchain.sh armhf
bash toolchains/linux/build-toolchain.sh armel
bash toolchains/linux/build-toolchain.sh i686
bash toolchains/linux/build-toolchain.sh ppc6xx
bash toolchains/linux/build-toolchain.sh riscv64

# Or build all 7 toolchains sequentially
bash toolchains/linux/build-toolchain.sh all

# Or package already built toolchains
bash toolchains/linux/build-toolchain.sh package all
```

Toolchains are installed into `build/toolchains/linux/<arch>`.

### 5.4 Manual Toolchain Setup (Step-by-Step)

If you prefer building toolchains manually using `musl-cross-make`:

1. **Clone musl-cross-make**:
   ```bash
   git clone https://github.com/richfelker/musl-cross-make.git
   cd musl-cross-make
   git checkout fe915821b652a7fa37b34a596f47d8e20bc72338
   ```

2. **Copy the time64 kernel compatibility patch**:
   ```bash
   mkdir -p patches/musl-1.2.6
   cp /path/to/nzbget/toolchains/linux/patches/musl-1.2.6/0010-time64-kernel-compat.patch patches/musl-1.2.6/
   ```

3. **Create `config.mak`**:
   ```makefile
   TARGET = x86_64-linux-musl
   OUTPUT = /path/to/nzbget/build/toolchains/linux/x86_64
   GCC_VER = 14.3.0
   BINUTILS_VER = 2.44
   MUSL_VER = 1.2.6
   COMMON_CONFIG += CFLAGS="-g0 -O2" CXXFLAGS="-g0 -O2" LDFLAGS="-s"
   ```

   For architectures requiring specific CPU baselines (preventing `SIGILL` on older devices):
   - **armhf**: `COMMON_CONFIG += --with-arch=armv7-a --with-fpu=vfpv3-d16 --with-float=hard --with-mode=thumb`
   - **armel**: `COMMON_CONFIG += --with-arch=armv5te --with-float=soft`
   - **ppc6xx**: `COMMON_CONFIG += --with-cpu=powerpc`
   - **riscv64**: `COMMON_CONFIG += --with-arch=rv64gc --with-abi=lp64d`
   - **i686**: `COMMON_CONFIG += --with-arch=i686 --with-tune=generic`

4. **Compile and install**:
   ```bash
   make
   make install
   ```

### 5.5 Building NZBGet with CMake Presets

Once the toolchains are installed in `build/toolchains/linux/<arch>` (or in a custom path pointed to by `NZBGET_LINUX_TOOLCHAIN_DIR`), build NZBGet using the presets in `CMakePresets.json`:

| Architecture | Release Preset | Debug Preset |
|---|---|---|
| `x86_64` | `cmake --preset ci-linux-x86_64` | `cmake --preset ci-linux-debug-x86_64` |
| `aarch64` | `cmake --preset ci-linux-aarch64` | `cmake --preset ci-linux-debug-aarch64` |
| `armhf` | `cmake --preset ci-linux-armhf` | `cmake --preset ci-linux-debug-armhf` |
| `armel` | `cmake --preset ci-linux-armel` | `cmake --preset ci-linux-debug-armel` |
| `i686` | `cmake --preset ci-linux-i686` | `cmake --preset ci-linux-debug-i686` |
| `ppc6xx` | `cmake --preset ci-linux-ppc6xx` | `cmake --preset ci-linux-debug-ppc6xx` |
| `riscv64` | `cmake --preset ci-linux-riscv64` | `cmake --preset ci-linux-debug-riscv64` |

#### Building with Docker (macOS, Windows WSL2, or Linux)
```bash
# Configure target (e.g. ARM64 Release)
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder cmake --preset ci-linux-aarch64

# Compile target
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder cmake --build --preset ci-linux-aarch64
```

#### Building Directly on Host
```bash
# Configure and build Release for ARM64
cmake --preset ci-linux-aarch64
cmake --build --preset ci-linux-aarch64
```

---

## 6. Automated Packaging (`platforms/linux/build.sh`)

The platform build runner `platforms/linux/build.sh` automates the end-to-end build and packaging lifecycle, generating release archives in `build/dist/`:

### 6.1 Binary Packages (`.tar.gz`)
Compiles NZBGet, downloads verified pre-built unpackers (`unrar` and `7za`), and packages a per-architecture release archive:

- **Via Docker**:
  ```bash
  # Single architecture
  docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh bin armhf release

  # All 7 architectures
  docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh bin all release
  ```

- **Directly on Host**:
  ```bash
  # Single architecture
  bash platforms/linux/build.sh bin armhf release

  # All 7 architectures
  bash platforms/linux/build.sh bin all release
  ```

Output: `build/dist/nzbget-<version>-bin-linux-<arch>.tar.gz`

### 6.2 Multi-Architecture Universal Installer (`.run`)
Assembles all 7 architecture binaries, unpackers, configuration files, WebUI, and certificates into a single universal self-extracting archive:

- **Via Docker**:
  ```bash
  docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh installer all release
  ```

- **Directly on Host**:
  ```bash
  bash platforms/linux/build.sh installer all release
  ```

Output: `build/dist/nzbget-<version>-bin-linux.run`

### 6.3 Full Build and Packaging Pipeline (`all`)
To compile all 7 architectures and generate both `.tar.gz` packages and the universal `.run` installer in one command:

```bash
docker run --rm -v "$(pwd):/workspace" nzbget-musl-builder bash platforms/linux/build.sh all all release
```

### 6.4 Native Linux Distribution Packages

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

---

## 7. Security & Hardening Features

All Linux release binaries are statically linked against musl libc with modern compiler and linker hardening:
- **Full RELRO & BIND_NOW**: `-Wl,-z,relro,-z,now`
- **Non-executable Stack**: `-Wl,-z,noexecstack`
- **Stack Protection**: `-fstack-protector-strong` and `-fstack-clash-protection` (on supported targets)
- **Control-Flow Integrity**:
  - Intel/AMD (`x86_64`): `-fcf-protection=full` (Intel CET)
  - ARM 64-bit (`aarch64`): `-mbranch-protection=standard` (PAC & BTI)
- **Source Fortification**: `-D_FORTIFY_SOURCE=2` or `=3` (enabled when building against libc with `__*_chk` symbol support)
- **C++ Standard Library Assertions**: `-D_GLIBCXX_ASSERTIONS` (for GCC Debug / RelWithDebInfo builds)
- **Visibility**: `-fvisibility=hidden` and `-fvisibility-inlines-hidden`
- **RTTI Disabled in Release**: `-fno-rtti`
- **Linker Section Pruning**: `-Wl,--gc-sections` and `-ffunction-sections` / `-fdata-sections`
- **Link-Time Optimization (LTO)**: Interprocedural Optimization enabled across release builds.
