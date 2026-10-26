# Android Build & Packaging Information

This directory contains Android platform-specific assets, build automation, auto-update scripts, and packaging utilities for NZBGet.

For comprehensive build instructions, CMake presets, toolchain details, and dependency options, see the primary documentation:
**[Android Platform Guide (docs/ANDROID.md)](../../docs/ANDROID.md)**

---

## Quick Reference

### Directory Layout
- `build.sh`: Platform driver script: configures and compiles per-architecture binaries, builds per-architecture binary packages and the multi-architecture `.run` installer.
- `installer.sh`: Dedicated self-extracting installer shell script template tailored for Android (Bionic, API 21+, multi-arch detection).
- `install-update.sh`: Native auto-update script executed by NZBGet WebUI to download, verify, and apply in-place Android updates.
- `package-info.json`: WebUI update metadata descriptor pointing to Android update feeds (`https://nzbget.com/info/nzbget-version-android.json`).

### Target Architectures
NZBGet targets **Android 5.0+ (API 21 Lollipop)** through modern **Android 15+** with pure LLVM Clang 19 and static `libc++`:

| Arch Parameter | Android ABI | Target Triple | Common Hardware |
|---|---|---|---|
| `arm64` | `arm64-v8a` | `aarch64-linux-android21` | Modern smartphones, 64-bit TV boxes (Nvidia Shield, Fire TV Cube) |
| `armv7` | `armeabi-v7a` | `armv7a-linux-androideabi21` | 32-bit legacy TV boxes, Fire TV sticks, NAS devices |
| `x86_64` | `x86_64` | `x86_64-linux-android21` | Android-x86 installations, Android Studio emulators |
| `x86` | `x86` | `i686-linux-android21` | Legacy 32-bit Intel Atom tablets, 32-bit emulators |

### CMake Presets
- Release: `ci-android-<arch>` (LTO OFF, hermetic dependencies from source/cache).
- Debug: `ci-android-debug-<arch>` (CMAKE_BUILD_TYPE Debug).

### Toolchain layout

`$NZBGET_ANDROID_TOOLCHAIN_DIR` (default `build/toolchains`, builder image `/opt/nzbget/android`):

```text
llvm/bin/clang
sysroot/usr/include
libcxx/<arch>/lib/libc++.a
libcxx/include/c++/v1
libcxx/api-level
```

`<arch>` is `arm64`, `armv7`, `x86_64`, or `x86`. The same directory also accepts the legacy names `android-llvm`, `android-sysroot`, and `android-libcxx`.

`libcxx/api-level` is a text file with the Android API level the static runtimes were built for (for example `21`). CMake reads it to default `ANDROID_API_LEVEL` and stops with an error if a different level is configured. When the file is missing, that check is skipped and the level defaults to `21`, so a custom toolchain should provide it. Older locations are also accepted: `api-level`, `android-libcxx/android-api-level` and `android-api-level`.

The [nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder) images already contain this layout.

### Building Packages with `build.sh`

```bash
# Build the 64-bit ARM binary package (default release) -> build/dist/*-bin-android-aarch64.tar.gz
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin arm64

# Build debug binary package for arm64
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin arm64 debug

# Build binary packages for all 4 architectures
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin all-arch

# Combine existing binary packages into a single multi-arch installer
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh installer all-arch

# Build installer for debug configuration
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh installer all-arch debug

# Build everything (binaries + installer) for the listed architectures, as a testing prerelease
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh all arm64 armv7 testing
```

Artifacts are written to `build/dist/`:
- `nzbget-<version>-bin-android-<aarch64|armhf|x86_64|i686>[-debug].tar.gz`: per-architecture binary package.
- `nzbget-<version>-bin-android[-debug].run`: multi-architecture installer (same format as the other platforms; `unrar`, `unrar7`, `7za`, `cacert.pem`, `pubkey.pem` included). The installer selects the binary by `uname -m`; use `--arch <name>` to override.

### Deploying & Testing on Device
- **Official Android App**: Install `nzbget.apk` from [nzbgetcom/android](https://github.com/nzbgetcom/android/releases), grant storage permission, place `nzbget-*.run` in `/sdcard/Download/`, and tap **Custom -> Install**.
- **Termux**: Run `sh nzbget-*.run` inside Termux and start `~/nzbget/nzbget -D`.
- **ADB Shell**: Push to `/data/local/tmp/`, run `sh nzbget-*.run --destdir /data/local/tmp/nzbget`, forward port with `adb forward tcp:6789 tcp:6789`.
