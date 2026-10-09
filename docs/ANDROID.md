# Building NZBGet for Android Platforms

This document provides a comprehensive guide for configuring, cross-compiling, testing, packaging, and deploying NZBGet for Android devices. NZBGet supports **Android 5.0 (API 21 Lollipop)** through modern **Android 15+** across four CPU architectures (`arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`).

NZBGet uses modern **CMake** (3.25+) with **FetchContent** to manage dependencies cleanly, deterministically, and hermetically.

---

## 1. Prerequisites & Host Environment

The Android cross-compilation pipeline is supported on Linux (`ubuntu-22.04`, `ubuntu-24.04`, Debian, Arch, Fedora) and macOS (Apple Silicon and Intel).

### Host Tools
Ensure the following tools are available on your build machine:
- **CMake** 3.25 or newer
- **Ninja** build system
- **curl**, **tar** (with xz support), **unzip**, **git**
- **tic** (terminfo compiler; used during ncurses build):
  - Ubuntu/Debian: `sudo apt install -y cmake ninja-build curl tar xz-utils unzip git ncurses-bin`
  - macOS (Homebrew): `brew install cmake ninja ncurses`
  - Arch Linux: `sudo pacman -S cmake ninja curl tar xz unzip git ncurses`

---

## 2. Hermetic Toolchain Design & Security

### Why Upstream LLVM 19 + NDK r26b?
Historically, Android builds relied on NDK standalone toolchains or Google's bundled `libc++_shared.so` / `libc++_static.a`. However, starting in NDK r27, Google dropped support for Android API levels below 24 (Android 7.0), breaking compatibility for Android 5.0–6.0 (API 21–23).

To guarantee **Android 5.0+ (API 21)** backward compatibility while enabling full **C++20** language features (`std::format`, ranges, concepts):
1. **Upstream LLVM 19.1.7**: NZBGet cross-compiles directly with upstream Clang 19 and LLD 19 targeting `*-linux-android21`.
2. **Bionic Sysroot**: Header files and platform libraries are sourced from Google's Android NDK r26b sysroot.
3. **Isolated Static libc++**: `toolchains/setup-android-toolchain.sh` compiles compiler-rt builtins and static `libc++.a`, `libc++abi.a`, and `libunwind.a` from the LLVM 19.1.7 source tree with:
   - `-D_LIBCPP_DISABLE_AVAILABILITY`: Disables OS-level availability guards, unlocking modern C++20 on Android API 21.
   - `-DLIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL=OFF`: Falls back to thread-local storage (TLS) keys, avoiding undefined `__cxa_thread_atexit_impl` references on Bionic API 21.
   - `-fvisibility=hidden -fvisibility-inlines-hidden`: Ensures all standard library symbols remain internal to the binary, preventing symbol collisions with Android system runtimes.
4. **Hermetic Linkage**: The toolchain links with `-fuse-ld=lld -rtlib=compiler-rt --unwindlib=none -nostdlib++` and wraps the static runtimes in `-Wl,--whole-archive libc++.a -Wl,--no-whole-archive libc++abi.a libunwind.a -ldl -lm -llog`.
5. **Runtime Hardening & Safety**: Consistent with macOS and Windows builds, NZBGet applies `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST` in `cmake/common.cmake` across all Clang compilation units, enforcing $O(1)$ runtime bounds and container integrity checks without performance degradation.
6. **Global Architecture Flags & SIGILL Prevention**: For 32-bit ARM (`armv7`), NZBGet enforces `-march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb`. Restricting the FPU to `vfpv3-d16` (16 double-precision registers `d0`-`d15`) guarantees that the compiler never emits NEON or 32-register instructions, completely preventing `SIGILL` (Illegal Instruction) crashes on non-NEON or early Cortex-A9/Tegra 2 hardware. For 32-bit x86 (`x86`), `-march=i686` is enforced. These architecture flags are applied globally across all runtimes (`compiler-rt`, `libc++`), CMake subprojects (`zlib`, `libxml2`, `rapidyenc`, `par2-turbo`, `boost`), and ExternalProject builds (`openssl`, `ncurses`), ensuring strict ABI consistency.
7. **File Offsets & Large File Support (>4GB)**:
   - **64-bit Android (`arm64`, `x86_64`)**: Bionic's LP64 ABI natively uses 64-bit file offsets (`off_t`). Downloads and files of arbitrary sizes (>4GB, multi-terabyte) work out of the box.
   - **32-bit Android (`armv7`, `x86`)**: Standard 32-bit file offsets apply to retain backward compatibility with Android 5.0/6.0 (API 21–23), where Bionic libc does not expose 64-bit C stdio stream functions (`fseeko64`, `fopen64`). Devices requiring multi-gigabyte monolithic archives on Android should run 64-bit builds.

### Cryptographic Checksum Verification
To ensure supply-chain security and reproducible builds, all external dependencies downloaded during toolchain setup and packaging are verified against cryptographic SHA256 checksums or immutable commit anchors:
- **LLVM 19.1.7 Release Archives**: Verified against published cryptographic SHA256 release checksums (`toolchains/setup-android-toolchain.sh`).
- **Android NDK r26b Archives**: Verified against Google's published repository hashes (`toolchains/setup-android-toolchain.sh`).
- **LLVM Runtime Sources (`compiler-rt`, `libc++`)**: Cloned from the official repository and anchored to the immutable Git commit SHA `cd708029e0b2869e80abe31ddb175f7c35361f90` (GPG-signed release tag `llvmorg-19.1.7` by Tobias Hieta), preventing risks from moved or compromised Git tags (`toolchains/setup-android-toolchain.sh`).
- **CA Root Certificates (`cacert.pem`)**: Downloaded alongside official `https://curl.se/ca/cacert.pem.sha256` and validated before bundling into installer packages (`platforms/android/build.sh`).

### Android API Level Synchronization & Safety
The minimum Android API level defaults to **21** (Android 5.0 Lollipop). To avoid subtle ABI mismatches between the prebuilt C++ runtimes (`libc++`, `compiler-rt`) and the main NZBGet binary, the build system enforces a single source of truth:
- `toolchains/setup-android-toolchain.sh` accepts `ANDROID_API_LEVEL` via environment or `--api-level=<num>` (e.g., `--api-level=21`) and records the targeted level in `build/toolchains/android-api-level`.
- `toolchains/android-llvm.cmake` checks this file during CMake configure time. If CMake's `ANDROID_API_LEVEL` cache variable deviates from the toolchain runtime API level, configuration fails with a clear diagnostic instructing how to re-target the toolchain runtimes or align the preset.
- If the toolchain API level changes, `toolchains/setup-android-toolchain.sh` automatically invalidates and cleans older incompatible runtime artifacts before rebuilding.

### Embedded ncurses Terminal Support
Android lacks standard terminal terminfo databases in `/usr/share/terminfo`. NZBGet builds a custom static `ncursesw 6.5` (`cmake/ncurses.cmake`) compiled with `--with-fallbacks=xterm,xterm-color,xterm-256color,xterm-16color,vt100,vt200,linux,ansi,screen,screen-256color,tmux,tmux-256color`. This ensures that NZBGet's interactive curses console mode (`nzbget -C`) works out of the box in ADB shells, SSH sessions, Termux, and tmux/screen without requiring external terminfo files.

### Third-Party Dependencies Managed from Source
Because Android targets are cross-compiled in a hermetic environment without package managers, all third-party dependencies are compiled from source and linked statically:
- **OpenSSL**: Static library (OpenSSL 3.5.x) built from source using `linux-*` configure targets (`linux-aarch64`, `linux-armv4`, `linux-x86_64-clang`, `linux-x86-clang`) rather than OpenSSL's upstream `android-*` targets. OpenSSL's internal `android-*` targets strictly require a full Google NDK installation (`$ANDROID_NDK_ROOT`) and invoke legacy NDK toolchains. In NZBGet's hermetic LLVM 19 toolchain, targeting `linux-*` while supplying `--target`, `--sysroot`, and `ANDROID_ARCH_FLAGS` enables Clang to automatically define Bionic preprocessor macros (`__ANDROID__=1`, `__ANDROID_API__=21`) and enforce safe architecture instructions, ensuring 100% Bionic compatibility without NDK dependencies.
- **zlib**: Compression library built statically via CMake FetchContent (`madler/zlib`).
- **libxml2**: XML parser built statically via CMake FetchContent from GNOME GitLab (configured without Python or tests).
- **Boost**: Header-only integration via CMake FetchContent (`Boost.JSON`, `Boost.Asio`, `Boost.Beast`), providing modern networking and JSON parsing without requiring precompiled Boost binaries.
- **par2-turbo**: High-performance PAR2 verification and repair engine built statically via CMake FetchContent.
- **rapidyenc**: SIMD-accelerated yEnc decoding engine built statically via CMake FetchContent.
- **ncursesw**: Wide-character curses library (`ncurses 6.5`, `cmake/ncurses.cmake`) built statically with embedded terminal fallbacks.

---

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
# 1. Setup the Hermetic Toolchain (one-time setup; accepts arm64, armv7, x86_64, x86, or all)
bash toolchains/setup-android-toolchain.sh arm64

# 2. Configure with CMake Preset
cmake --preset ci-android-arm64

# Or for Debug builds:
# cmake --preset ci-android-debug-arm64

# 3. Build with Ninja
cmake --build --preset ci-android-arm64 -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

The resulting binary will be located at `build/ci-android-arm64/nzbget`.

---

## 4. Packaging with `platforms/android/build.sh`

The runner script `platforms/android/build.sh` automates toolchain verification, binary compilation, and packaging into standard distribution artifacts.

### Command Reference

```bash
# Build 64-bit ARM binary package (Release)
bash platforms/android/build.sh bin arm64 release

# Build 64-bit ARM binary package (Debug)
bash platforms/android/build.sh bin arm64 debug

# Build binary packages for all 4 architectures
bash platforms/android/build.sh bin all-arch release

# Build unified multi-architecture self-extracting .run installer (combining all 4 ABIs)
bash platforms/android/build.sh installer all-arch release

# Build both binaries and installer in one command
bash platforms/android/build.sh all all-arch release
```

### Generated Artifacts in `build/dist/`:
- `nzbget-<version>-bin-android-<arch>[-debug].tar.gz`: Standalone archive containing `nzbget` binary, WebUI assets, documentation, and `nzbget.conf.template`.
- `nzbget-<version>-bin-android[-debug].run`: Multi-architecture self-extracting installer.
  - Automatically identifies device CPU architecture at runtime.
  - Bundles tested unpackers (`7za`, `unrar` v6 & v7) and Mozilla CA root certificates.
  - Supports command line arguments: `--destdir <dir>`, `--arch <arch>`, `--list`, `--info`, and in-place updates via `install-update.sh` and `pubkey.pem`.

---

## 5. Deploying and Running NZBGet on Android

### Option A: Running in Termux (Recommended for Phones & Tablets)
1. Install **Termux** (from F-Droid or GitHub releases; do not use Google Play version).
2. Copy or download the `.run` package inside Termux:
   ```bash
   cd ~
   sh nzbget-<version>-bin-android.run
   ```
3. Start NZBGet:
   ```bash
   # Start as a daemon (background process):
   ~/nzbget/nzbget -D -c ~/nzbget/nzbget.conf

   # Or start in curses console mode:
   ~/nzbget/nzbget -C -c ~/nzbget/nzbget.conf
   ```
4. Access the WebUI in your mobile browser at `http://127.0.0.1:6789` (default credentials: `nzbget` / `tegbzn678`).

### Option B: Running via ADB / Headless Shell (Nvidia Shield, Android TV, NAS)
1. Push the installer to `/data/local/tmp` (a standard Android directory with execute permissions):
   ```bash
   adb push nzbget-<version>-bin-android.run /data/local/tmp/
   ```
2. Install NZBGet:
   ```bash
   adb shell "cd /data/local/tmp && sh nzbget-<version>-bin-android.run --destdir /data/local/tmp/nzbget"
   ```
3. Launch the daemon:
   ```bash
   adb shell "/data/local/tmp/nzbget/nzbget -D -c /data/local/tmp/nzbget/nzbget.conf"
   ```
4. Forward the WebUI port to your development machine:
   ```bash
   adb forward tcp:6789 tcp:6789
   ```
   Open `http://localhost:6789` in your desktop browser.

### Option C: Official Companion Android App (`nzbgetcom/android`)
The companion Android launcher app is maintained in the [nzbgetcom/android](https://github.com/nzbgetcom/android) repository.

1. Download and install `nzbget.apk` from [nzbgetcom/android releases](https://github.com/nzbgetcom/android/releases).
2. Grant storage permissions in Android Settings:
   - *Settings -> Apps -> NZBGet -> Permissions -> Files and Media (or Storage) -> Allow*.
3. Copy the built `nzbget-*.run` installer into the device's `Download/` directory (`/sdcard/Download/`).
4. In the NZBGet app, tap **Custom**, choose the detected `.run` installer, and tap **Install**.
5. Tap **Start Daemon** to launch NZBGet and browse via the built-in WebView.

---

## 6. Troubleshooting & Best Practices

### W^X and `noexec` Mount Restrictions on Android 10+
On modern Android versions (API 29+), executing binaries directly from shared external storage (`/sdcard` or `/storage/emulated/0`) is blocked by SELinux and kernel mount options (`noexec`).
- **Solution**: Always install and execute NZBGet from an app's private internal directory (`/data/data/com.termux/files/home` in Termux, `/data/data/com.nzbget.android/` in the companion app, or `/data/local/tmp` via ADB). NZBGet can still download files to shared storage by configuring `DestDir=/sdcard/Download/completed` in `nzbget.conf`.

### Terminal / Curses Display Issues
If terminal colors or borders look distorted when running `nzbget -C` in ADB or SSH:
```bash
export TERM=xterm-256color
~/nzbget/nzbget -C
```
The embedded ncurses fallbacks in NZBGet automatically provide standard capability maps for `xterm`, `screen`, `tmux`, `vt100`, and `linux`.

### Battery Optimizations & Background Killing
Android may terminate background processes when the screen turns off.
- In Android Settings -> Apps -> NZBGet (or Termux), set **Battery usage** to **Unrestricted**.
- In Termux, run `termux-wake-lock` to keep the CPU active during large downloads and unpack operations.
