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

### CMake Presets
- Release: `ci-freebsd-x86_64`, `ci-freebsd-aarch64`
- Debug: `ci-freebsd-debug-x86_64`, `ci-freebsd-debug-aarch64`

### Building Packages with `build.sh`

```bash
# Build binary package for x86_64 (release) -> build/dist/*-bin-freebsd-x86_64.tar.gz
bash platforms/freebsd/build.sh bin x86_64 release

# Build debug binary package for aarch64
bash platforms/freebsd/build.sh bin aarch64 debug

# Build binary packages for all architectures
bash platforms/freebsd/build.sh bin all-arch release

# Combine existing binary packages into a single multi-arch installer (.run)
bash platforms/freebsd/build.sh installer all-arch release

# Build everything in one go
bash platforms/freebsd/build.sh all all-arch release
```

Artifacts are written to `build/dist/`:
- `nzbget-<version>-bin-freebsd-<arch>[-debug].tar.gz`: per-architecture binary package.
- `nzbget-<version>-bin-freebsd[-debug].run`: multi-architecture installer bundling unpackers (`7za`, `unrar`), root CA certificates (`cacert.pem`), and the update signature key (`pubkey.pem`). Automatically selects the target CPU architecture on execution.
