# Building NZBGet for Android Platforms

How to configure, cross-compile, test, package and deploy NZBGet for Android. NZBGet supports **Android 5.0 (API 21 Lollipop)** and newer on four CPU architectures: `arm64-v8a`, `armeabi-v7a`, `x86_64` and `x86`. External dependencies are fetched and built from source with **CMake** (3.25+) and **FetchContent**.

## 1. Prerequisites & Host Environment

Cross-compilation uses the Android image published by [nzbgetcom/nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder):

`ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0`

The tag suffix is the `VERSION` file in that repository (currently `1.0`). The image is `linux/amd64` and already contains LLVM 19.1.7, the API 21 sysroot, the static libc++ runtimes, CMake, Ninja, and the host tools the build needs. On Apple Silicon pass `--platform linux/amd64`. Mount the NZBGet sources on `/src`. The image sets `NZBGET_ANDROID_TOOLCHAIN_DIR=/opt/nzbget/android`.

## 2. Hermetic Toolchain Design & Security

### Why Upstream LLVM 19 + NDK r26b?

Android builds historically used NDK standalone toolchains or Google's bundled `libc++_shared.so` / `libc++_static.a`, but NDK r27 dropped support for Android API levels below 24 (Android 7.0), breaking Android 5.0–6.0 (API 21–23). To keep **Android 5.0+ (API 21)** compatibility while enabling **C++20** (`std::format`, ranges, concepts):

1. **Upstream LLVM 19.1.7**: cross-compilation uses upstream Clang 19 and LLD 19 targeting `*-linux-android21`.
2. **Bionic Sysroot**: headers and platform libraries come from the Android NDK r26b sysroot.
3. **Isolated Static libc++**: the Android image includes compiler-rt builtins and static `libc++.a`, `libc++abi.a` and `libunwind.a` built from the LLVM 19.1.7 source tree with `-D_LIBCPP_DISABLE_AVAILABILITY` (unlocks modern C++20 on API 21), `-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF` (uses TLS keys instead of the missing `__cxa_thread_atexit_impl`) and `-fvisibility=hidden -fvisibility-inlines-hidden` (keeps the bundled runtime symbols internal, avoiding collisions with Android system libraries).
4. **Hermetic Linkage**: the toolchain links with `-fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -nostdlib++` and `-Wl,--whole-archive libc++.a -Wl,--no-whole-archive libc++abi.a libunwind.a -ldl -lm -llog`.
5. **Runtime Hardening**: as on macOS and Windows, `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST` is applied in `cmake/common.cmake` to every Clang compilation unit, enforcing O(1) runtime bounds and container integrity checks.
6. **Global Architecture Flags (SIGILL Prevention)**: 32-bit ARM (`armv7`) builds use `-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb`; limiting the FPU to `vfpv3-d16` (16 double-precision registers `d0`-`d15`) keeps NEON and 32-register instructions out of the binary, preventing SIGILL (Illegal Instruction) crashes on non-NEON and early Cortex-A9/Tegra 2 hardware. 32-bit x86 (`x86`) uses `-march=i686`. The flags are applied to all runtimes (`compiler-rt`, `libc++`), CMake subprojects (`zlib`, `libxml2`, `rapidyenc`, `par2-turbo`, `boost`) and ExternalProject builds (`openssl`, `ncurses`).
7. **File Offsets (>4GB)**: the 64-bit targets (`arm64`, `x86_64`) use Bionic's LP64 64-bit `off_t` and handle files of any size. The 32-bit targets (`armv7`, `x86`) keep 32-bit file offsets for compatibility with API 21–23, where Bionic libc has no `fseeko64`/`fopen64`; use a 64-bit build for multi-gigabyte monolithic archives.

### Cryptographic Checksum Verification

Every dependency downloaded while the Android image is built, and while packages are assembled, is verified against SHA256 checksums or immutable commit anchors:

- **LLVM 19.1.7 release archives**: published SHA256 release checksums, checked in [nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder).
- **Android NDK r26b archives**: Google's published repository hashes, checked in the same repository.
- **LLVM runtime sources (`compiler-rt`, `libc++`)**: the official repository pinned to the immutable commit `cd708029e0b2869e80abe31ddb175f7c35361f90` (GPG-signed release tag `llvmorg-19.1.7` by Tobias Hieta), so moved or compromised tags cannot affect the image.
- **CA root certificates (`cacert.pem`)**: downloaded together with the official `https://curl.se/ca/cacert.pem.sha256` and validated before bundling into the installer packages (`platforms/android/build.sh`).

### Android API Level Synchronization

The minimum API level is **21** (Android 5.0 Lollipop). The Android image records that level next to the static runtimes. `toolchains/android/android-llvm.cmake` reads it at configure time and fails if CMake's `ANDROID_API_LEVEL` does not match.

### Third-Party Dependencies

Android targets have no package manager, so every dependency is compiled from source and linked statically:

- **OpenSSL** (3.5.x): built from source with the `linux-*` configure targets (`linux-aarch64`, `linux-armv4`, `linux-x86_64-clang`, `linux-x86-clang`) instead of the upstream `android-*` targets, which require a full Google NDK installation (`$ANDROID_NDK_ROOT`) and invoke legacy NDK toolchains. Supplying `--target`, `--sysroot` and `ANDROID_ARCH_FLAGS` makes Clang define the Bionic preprocessor macros (`__ANDROID__=1`, `__ANDROID_API__=21`) and emit architecture-safe instructions.
- **zlib**: compression library from CMake FetchContent (`madler/zlib`).
- **libxml2**: XML parser from CMake FetchContent (GNOME GitLab), configured without Python bindings or tests.
- **Boost**: header-only FetchContent integration (`Boost.JSON`, `Boost.Asio`, `Boost.Beast`).
- **par2-turbo**: PAR2 verification and repair engine from CMake FetchContent.
- **rapidyenc**: SIMD-accelerated yEnc decoding from CMake FetchContent.
- **ncursesw**: `ncurses 6.5` (`cmake/ncurses.cmake`), static, compiled with `--with-fallbacks=xterm,xterm-color,xterm-256color,xterm-16color,vt100,vt200,linux,ansi,screen,screen-256color,tmux,tmux-256color` because Android has no terminfo databases in `/usr/share/terminfo`; the curses console mode (`nzbget -C`) therefore works in ADB shells, SSH sessions, Termux and tmux/screen.

## 3. Quick Start with CMake Presets

### Target Architectures & Presets Reference

| Architecture | Android ABI | Target Triple | File Offsets / LFS | Configure Preset (Release) | Configure Preset (Debug) | Typical Devices |
|---|---|---|---|---|---|---|
| **arm64** | `arm64-v8a` | `aarch64-linux-android21` | 64-bit (>4GB) | `ci-android-arm64` | `ci-android-debug-arm64` | Modern 64-bit phones, Nvidia Shield, Google TV |
| **armv7** | `armeabi-v7a` | `armv7a-linux-androideabi21` | 32-bit (API 21) | `ci-android-armv7` | `ci-android-debug-armv7` | 32-bit TV boxes, Fire TV sticks, legacy NAS |
| **x86_64** | `x86_64` | `x86_64-linux-android21` | 64-bit (>4GB) | `ci-android-x86_64` | `ci-android-debug-x86_64` | Android-x86, Android Studio emulators |
| **x86** | `x86` | `i686-linux-android21` | 32-bit (API 21) | `ci-android-x86` | `ci-android-debug-x86` | 32-bit Intel Atom tablets, legacy emulators |

### Step-by-Step Compilation

```bash
# Release: configure
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  cmake --preset ci-android-arm64
# Release: build
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  cmake --build --preset ci-android-arm64

# Debug: configure
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  cmake --preset ci-android-debug-arm64
# Debug: build
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  cmake --build --preset ci-android-debug-arm64
```

The resulting binary is located at `build/ci-android-arm64/nzbget` (Release) or `build/ci-android-debug-arm64/nzbget` (Debug).

## 4. Packaging with `platforms/android/build.sh`

The runner script `platforms/android/build.sh` verifies the toolchain, compiles the binary and packages the distribution artifacts.

```bash
# Build 64-bit ARM binary package (Release)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin arm64 release

# Build 64-bit ARM binary package (Debug)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin arm64 debug

# Build binary packages for all 4 architectures
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh bin all-arch release

# Build unified multi-architecture self-extracting .run installer (combining all 4 ABIs)
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh installer all-arch release

# Build both binaries and installer in one command
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/nzbgetcom/nzbget-musl-builder:android-1.0 \
  bash platforms/android/build.sh all all-arch release
```

Generated artifacts in `build/dist/`:

- `nzbget-<version>-bin-android-<arch>[-debug].tar.gz` - the `nzbget` binary, WebUI assets, documentation and `nzbget.conf.template`; `<arch>` is one of `aarch64`, `armhf`, `x86_64`, `i686`.
- `nzbget-<version>-bin-android[-debug].run` - multi-architecture self-extracting installer which detects the device CPU architecture at runtime, bundles the tested unpackers (`7za`, `unrar` v6 & v7) and Mozilla CA root certificates (`cacert.pem`), takes the command line arguments `--destdir <dir>`, `--arch <arch>`, `--list`, `--info`, and supports in-place updates via `install-update.sh` and `pubkey.pem`.

## 5. Deploying and Running NZBGet on Android

### Option A: Termux (phones and tablets)

Install **Termux** from F-Droid or GitHub releases (not the Google Play version), then download or copy the `.run` package, install and start NZBGet:

```bash
cd ~
sh nzbget-<version>-bin-android.run
~/nzbget/nzbget -D -c ~/nzbget/nzbget.conf   # daemon (background process)
~/nzbget/nzbget -C -c ~/nzbget/nzbget.conf   # or curses console mode
```

The WebUI is at `http://127.0.0.1:6789` (default credentials: `nzbget` / `tegbzn678`).

### Option B: ADB / Headless Shell (Nvidia Shield, Android TV, NAS)

```bash
adb push nzbget-<version>-bin-android.run /data/local/tmp/   # /data/local/tmp has execute permissions
adb shell "cd /data/local/tmp && sh nzbget-<version>-bin-android.run --destdir /data/local/tmp/nzbget"
adb shell "/data/local/tmp/nzbget/nzbget -D -c /data/local/tmp/nzbget/nzbget.conf"
adb forward tcp:6789 tcp:6789   # then open http://localhost:6789 in the desktop browser
```

### Option C: Official Companion Android App (`nzbgetcom/android`)

The companion launcher app is maintained in the [nzbgetcom/android](https://github.com/nzbgetcom/android) repository.

1. Download and install `nzbget.apk` from [nzbgetcom/android releases](https://github.com/nzbgetcom/android/releases).
2. Grant storage permissions in Android Settings: *Settings -> Apps -> NZBGet -> Permissions -> Files and Media (or Storage) -> Allow*.
3. Copy the built `nzbget-*.run` installer into the device's `Download/` directory (`/sdcard/Download/`).
4. In the NZBGet app tap **Custom**, choose the detected `.run` installer and tap **Install**.
5. Tap **Start Daemon** to launch NZBGet and browse via the built-in WebView.

## 6. Troubleshooting & Best Practices

### W^X and `noexec` Mount Restrictions on Android 10+

On Android 10+ (API 29+) SELinux and the kernel `noexec` mount option block the execution of binaries from shared external storage (`/sdcard`, `/storage/emulated/0`). Install and run NZBGet from an app's private internal directory instead: `/data/data/com.termux/files/home` (Termux), `/data/data/com.nzbget.android/` (companion app) or `/data/local/tmp` (ADB). Downloads can still go to shared storage by setting `DestDir=/sdcard/Download/completed` in `nzbget.conf`.

### Terminal / Curses Display Issues

If terminal colors or borders look distorted when running `nzbget -C` in ADB or SSH:

```bash
export TERM=xterm-256color
~/nzbget/nzbget -C
```

### Battery Optimizations & Background Killing

Android may terminate background processes when the screen turns off.

- In Android Settings -> Apps -> NZBGet (or Termux) set **Battery usage** to **Unrestricted**.
- In Termux run `termux-wake-lock` to keep the CPU active during large downloads and unpack operations.
