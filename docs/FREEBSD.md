# Building NZBGet for FreeBSD

Guide for configuring, building, testing, and packaging NZBGet on FreeBSD 13.0+ across `x86_64` (amd64) and `aarch64` (arm64).

Supported workflows:
1. **Native Build**: Direct compilation on FreeBSD using system tools and packages.
2. **Cross-Compilation**: Cross-compilation from Linux or macOS using LLVM and a FreeBSD sysroot.

---

## 1. Supported Architectures

| Architecture | FreeBSD Arch | Target Triple |
|---|---|---|
| `x86_64` | `amd64` | `x86_64-pc-freebsd13` |
| `aarch64` | `arm64` | `aarch64-pc-freebsd13` |

---

## 2. Native Build on FreeBSD

### 2.1 Prerequisites

FreeBSD base includes Clang, `libc++`, and `zlib`. Install required tools and libraries via `pkg`:

```bash
pkg install -y git cmake ninja openssl libxml2 boost-all ncurses
```

*Note*: For headless environments (jails, servers) without terminal UI, pass `-DDISABLE_CURSES=ON` to CMake; in that case, `ncurses` is not required.

### 2.2 Build

#### Local Development (System Packages)

Uses system packages dynamically for fast compilation:

```bash
# Release build
cmake --preset release
cmake --build --preset release

# Debug build with unit tests
cmake --preset debug-tests
cmake --build --preset debug-tests
ctest --preset debug-tests --output-on-failure
```

#### Hermetic Static Release (FetchContent)

Compiles all third-party dependencies from source, producing a fully static binary:

```bash
cmake -B build -G Ninja -DBUILD_DEPS_FROM_SOURCE=ON -DENABLE_STATIC=ON
cmake --build build
```

Binary output: `build/nzbget`.

Run options:
```bash
./build/nzbget -s -c nzbget.conf   # Console mode
./build/nzbget -D -c nzbget.conf   # Daemon mode
```

---

## 3. Cross-Compilation (from Linux / macOS)

### 3.1 Host Prerequisites

Requires LLVM (Clang, LLD), CMake, and archive utilities:

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y clang lld llvm xz-utils curl cmake ninja-build git
```

### 3.2 Prepare FreeBSD Sysroot

Download and extract official FreeBSD base distributions (`base.txz`):

```bash
# x86_64 sysroot (build/toolchains/freebsd/sysroot)
bash toolchains/freebsd/build-toolchain.sh amd64 13.0

# aarch64 sysroot (build/toolchains/freebsd/sysroot-aarch64)
bash toolchains/freebsd/build-toolchain.sh arm64 13.0
```

### 3.3 Build with CMake Presets

```bash
# Release (Static + LTO)
cmake --preset ci-freebsd-x86_64
cmake --build --preset ci-freebsd-x86_64

cmake --preset ci-freebsd-aarch64
cmake --build --preset ci-freebsd-aarch64

# Debug
cmake --preset ci-freebsd-debug-x86_64
cmake --build --preset ci-freebsd-debug-x86_64

cmake --preset ci-freebsd-debug-aarch64
cmake --build --preset ci-freebsd-debug-aarch64
```

Binaries are located at `build/ci-freebsd-<arch>/nzbget`.

---

## 4. Packaging with `platforms/freebsd/build.sh`

Builds distribution packages (`.tar.gz` and `.run`):

```bash
# Build per-architecture binary packages (.tar.gz)
bash platforms/freebsd/build.sh bin all-arch release

# Build unified multi-arch installer (.run)
bash platforms/freebsd/build.sh installer all-arch release

# Build all packages in one command
bash platforms/freebsd/build.sh all all-arch release
```

Output directory: `build/dist/`
- `nzbget-<version>-bin-freebsd-x86_64.tar.gz`
- `nzbget-<version>-bin-freebsd-aarch64.tar.gz`
- `nzbget-<version>-bin-freebsd.run` (multi-arch self-extracting installer)

---

## 5. CMake Presets Reference

| Preset Name | Architecture | Build Type | Linkage | Configuration |
|---|---|---|---|---|
| `release` | Host (native) | Release | Dynamic | Host libraries |
| `minsizerel` | Host (native) | MinSizeRel | Dynamic | Host libraries |
| `reldebinfo` | Host (native) | RelWithDebInfo | Dynamic | Host libraries |
| `release-lto` | Host (native) | Release + LTO | Dynamic | Host libraries |
| `release-tests` | Host (native) | Release | Dynamic | Host libraries, tests enabled |
| `debug` | Host (native) | Debug | Dynamic | Host libraries |
| `debug-tests` | Host (native) | Debug | Dynamic | Host libraries, tests enabled |
| `debug-asan` | Host (native) | Debug | Dynamic | ASan + UBSan |
| `debug-tsan` | Host (native) | Debug | Dynamic | TSan |
| `ci-freebsd-x86_64` | `x86_64` | Release + LTO | Static | Hermetic cross-build via sysroot |
| `ci-freebsd-aarch64` | `aarch64` | Release + LTO | Static | Hermetic cross-build via sysroot |
| `ci-freebsd-debug-x86_64` | `x86_64` | Debug | Static | Hermetic cross-build via sysroot |
| `ci-freebsd-debug-aarch64` | `aarch64` | Debug | Static | Hermetic cross-build via sysroot |

---

## 6. Deployment on FreeBSD

### 6.1 Installation

```bash
sh nzbget-<version>-bin-freebsd.run --destdir /usr/local/nzbget
```

### 6.2 Service Setup (`rc.d`)

Create `/usr/local/etc/rc.d/nzbget`:

```sh
#!/bin/sh
#
# PROVIDE: nzbget
# REQUIRE: DAEMON NETWORKING
# KEYWORD: shutdown

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

Enable and start:
```bash
chmod 755 /usr/local/etc/rc.d/nzbget
sysrc nzbget_enable="YES"
service nzbget start
```

---

## 7. Unpackers (UnRAR & 7-Zip)

Pre-compiled static unpackers (`unrar`, `7za`) are automatically bundled inside `.run` packages.

To use system unpackers instead:
```bash
pkg install -y unrar 7-zip
```

Configure in `nzbget.conf`:
```ini
UnrarCmd=/usr/local/bin/unrar
SevenZipCmd=/usr/local/bin/7z
```
