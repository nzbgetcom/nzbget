# Building NZBGet for FreeBSD

This document describes how to configure, build, test and package NZBGet for FreeBSD 13.0+ on `x86_64` (amd64) and `aarch64` (arm64).

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

Cross-compilation uses the FreeBSD images published by [nzbgetcom/nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder). The tag suffix is the `VERSION` file in that repository (currently `1.0`). Images are `linux/amd64`; on Apple Silicon pass `--platform linux/amd64`. Mount the NZBGet sources on `/src`.

| Architecture | Image |
|---|---|
| `x86_64` | `ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-x86_64-1.0` |
| `aarch64` | `ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-aarch64-1.0` |
| both architectures | `ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0` |

The image contains Clang 19 and the FreeBSD 13.0 base sysroot (`libc`, `libthr`, `libm`, and the OS headers) under `/opt/nzbget/freebsd`. That sysroot ships libc++ 11, which has no `<coroutine>`, `<format>`, `<span>` or `<concepts>`. `toolchains/freebsd/build-cxx-runtime.sh`, run inside the image, builds the static libc++/libc++abi/libunwind and compiler-rt builtins from pinned LLVM 19.1.7 sources and leaves them next to that sysroot. It does not download a sysroot. The target stays FreeBSD 13.0: binaries use the FreeBSD 13.0 C library, and only the C++ runtime comes from LLVM.

### 3.1 Build with CMake Presets

One container builds the C++ runtime and then NZBGet. A second `docker run` does not keep the runtime, because it is written inside the image filesystem.

```bash
# x86_64 release
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-x86_64-1.0 \
  bash -c 'bash toolchains/freebsd/build-cxx-runtime.sh x86_64 13.0 && cmake --preset ci-freebsd-x86_64 && cmake --build --preset ci-freebsd-x86_64'

# aarch64 release
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-aarch64-1.0 \
  bash -c 'bash toolchains/freebsd/build-cxx-runtime.sh aarch64 13.0 && cmake --preset ci-freebsd-aarch64 && cmake --build --preset ci-freebsd-aarch64'
```

Debug presets are `ci-freebsd-debug-x86_64` and `ci-freebsd-debug-aarch64`. Binaries are located at `build/ci-freebsd-<arch>/nzbget`.

---

## 4. Packaging with `platforms/freebsd/build.sh`

Builds distribution packages (`.tar.gz` and `.run`):

`platforms/freebsd/build.sh` builds the C++ runtime when it is missing, then compiles and packages. Use `all-1.0` when one command builds both architectures.

```bash
# Per-architecture binary packages (.tar.gz)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/freebsd/build.sh bin all-arch release

# One architecture
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-x86_64-1.0 \
  bash platforms/freebsd/build.sh bin x86_64 release

# Unified multi-arch installer (.run)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/freebsd/build.sh installer all-arch release

# All packages in one command
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
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
# Quit the running server.
stop_cmd="${command} -Q -c ${nzbget_conf}"
# Ask the running server for its status (see the note below).
status_cmd="${command} -L S -c ${nzbget_conf}"

run_rc_command "$1"
```

`status_cmd` uses the `S` selector of the `-L` (`--list`) option (`nzbget -L S`, see `nzbget -h`): it asks the running server for its status and exits with `0` when the server responds, with a non-zero code when it does not. This is what the rc.d `status` action expects; without a selector the same option lists the files of the download queue instead.

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

Depending on the FreeBSD release, the 7-Zip package installs the binary as `7zz` instead of `7z`. Check with `pkg info -l 7-zip` and set `SevenZipCmd` to the installed path.
