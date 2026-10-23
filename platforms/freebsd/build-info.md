# FreeBSD Build & Packaging Information

This directory contains FreeBSD platform-specific assets, build automation, and packaging utilities for NZBGet.

For comprehensive build instructions, CMake presets, toolchain details, and dependency options, see the primary documentation:
**[FreeBSD Platform Guide (docs/FREEBSD.md)](../../docs/FREEBSD.md)**

---

## Quick Reference

### Directory Layout
- `build.sh`: Platform driver script: compiles binaries, creates per-architecture tarballs, and assembles the multi-architecture `.run` installer.
- `installer.sh`: Self-extracting installer shell script template tailored for FreeBSD (FreeBSD 13+, multi-arch detection).
- `install-update.sh`: Auto-update script executed by NZBGet WebUI to download, verify, and apply in-place updates.
- `package-info.json`: WebUI update metadata descriptor pointing to FreeBSD update feeds.

### Target Architectures
NZBGet targets **FreeBSD 13.0+** across two CPU architectures:

| Arch Parameter | Canonical FreeBSD Name | Target Triple |
|---|---|---|
| `x86_64` / `amd64` | `amd64` | `x86_64-pc-freebsd13` |
| `aarch64` / `arm64` | `arm64` | `aarch64-pc-freebsd13` |

The base C library and headers come from a pinned FreeBSD 13.0 sysroot, while the
C++ runtime is a static libc++/libc++abi/libunwind built from LLVM 19.1.7 sources
for the FreeBSD 13.0 target. Binaries are statically linked and require no
libc++ on the target system.

### CMake Presets
- Release: `ci-freebsd-x86_64`, `ci-freebsd-aarch64`
- Debug: `ci-freebsd-debug-x86_64`, `ci-freebsd-debug-aarch64`

### Toolchain layout

`$FREEBSD_PREFIX` (default `build/toolchains/freebsd`, builder image `/opt/nzbget/freebsd`):

```text
sysroot/usr/include              x86_64
sysroot-x86_64/usr/include       x86_64, alternate name
sysroot-aarch64/usr/include      aarch64
libcxx/include/c++/v1            x86_64 C++ runtime
libcxx/lib/libc++.a
libcxx-aarch64/include/c++/v1    aarch64 C++ runtime
libcxx-aarch64/lib/libc++.a
```

The [nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder) images already contain the sysroot. `toolchains/freebsd/build-cxx-runtime.sh` builds the C++ runtime next to it.

### Building Packages with `build.sh`

One architecture uses its own image. Commands that build both architectures use `all-1.0`.

```bash
# Build binary package for x86_64 (release) -> build/dist/*-bin-freebsd-x86_64.tar.gz
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-x86_64-1.0 \
  bash platforms/freebsd/build.sh bin x86_64 release

# Build debug binary package for aarch64
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:freebsd-aarch64-1.0 \
  bash platforms/freebsd/build.sh bin aarch64 debug

# Build binary packages for all architectures
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/freebsd/build.sh bin all-arch release

# Combine existing binary packages into a single multi-arch installer (.run)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/freebsd/build.sh installer all-arch release

# Build everything in one go
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:all-1.0 \
  bash platforms/freebsd/build.sh all all-arch release
```

Artifacts are written to `build/dist/`:
- `nzbget-<version>-bin-freebsd-<arch>[-debug].tar.gz`: per-architecture binary package.
- `nzbget-<version>-bin-freebsd[-debug].run`: multi-architecture installer bundling unpackers (`7za`, `unrar`), root CA certificates (`cacert.pem`), and the update signature key (`pubkey.pem`). Automatically selects the target CPU architecture on execution.
