# Building NZBGet for macOS

How to configure, build, test and package NZBGet for macOS (Apple Silicon `arm64` and Intel `x86_64`). External dependencies are fetched and built from source with **CMake** (3.25+) and **FetchContent**.

## 1. Prerequisites

### Basic Tools

```bash
xcode-select --install   # Xcode Command Line Tools (or full Xcode)
brew install cmake       # CMake 3.25 or newer
brew install ninja       # Ninja, the generator used by all CMake presets (override with -G)
```

### Optional Dependencies & Tools

```bash
brew install openssl@3 zlib libxml2 boost   # system libraries for local development
brew install create-dmg                     # used by platforms/macos/sign/nzbget-sign.sh to build .dmg packages
brew install llvm@19                        # LLVM toolchain used in release CI for hermetic builds with static libc++
```

## 2. Dependency Management: Source vs System

Two dependency resolution models are available, controlled by the CMake cache variable `BUILD_DEPS_FROM_SOURCE`:

| Option | Value | Behavior | Recommended For |
|---|---|---|---|
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (default in dev presets) | Tries to find system/Homebrew libraries first (`FindOpenSSL`, `FindZLIB`, `FindLibXml2`, `FindBoost`). If not found, falls back automatically to FetchContent. `rapidyenc` and `par2-turbo` are always built from source via FetchContent. | Local development, rapid iteration |
| `BUILD_DEPS_FROM_SOURCE` | `ON` (default in `ci-*` presets) | Pure hermetic build: fetches and compiles all dependencies from source via CMake FetchContent. | CI, release builds, redistributable binaries |

### Dependencies Managed via FetchContent

- **OpenSSL**: built from source via `jimmy-park/openssl-cmake` (3.x static library).
- **zlib**: built from `madler/zlib` (static library).
- **libxml2**: built from GNOME GitLab (configured without Python/tests, static library).
- **Boost**: header-only integration (Boost.JSON, Boost.Asio).
- **par2-turbo**: PAR2 verification and repair (static library).
- **rapidyenc**: SIMD-accelerated yEnc decoding (static library).

## 3. Quick Start: Using CMake Presets

All presets in `CMakePresets.json` use the **Ninja** generator; pass `-G <generator>` to the `cmake` invocation to use another one (Xcode, Unix Makefiles).

```bash
# Debug build -> build/debug/nzbget
cmake --preset debug
cmake --build --preset debug

# Release build -> build/release/nzbget
cmake --preset release
cmake --build --preset release

# Unit tests
cmake --preset debug-tests
cmake --build --preset debug-tests
ctest --preset debug-tests --output-on-failure

# Sanitizer builds
cmake --preset debug-asan && cmake --build --preset debug-asan   # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset debug-tsan && cmake --build --preset debug-tsan   # ThreadSanitizer
```

## 4. Cross-Compilation and CI Presets

For cross-compiling or building standalone redistributable binaries:

| Preset Name | Target Architecture | Source Deps | LTO | Compiler | Notes |
|---|---|---|---|---|---|
| `ci-macos-arm64` | `arm64` (Apple Silicon) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 11.0 (Big Sur), curses disabled |
| `ci-macos-x64` | `x86_64` (Intel Mac) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 10.14 (Mojave), curses disabled |
| `ci-macos-debug-arm64` | `arm64` (Apple Silicon) | `ON` | `OFF` | LLVM Clang 19 | Debug build, static libc++, deployment target macOS 11.0 |
| `ci-macos-debug-x64` | `x86_64` (Intel Mac) | `ON` | `OFF` | LLVM Clang 19 | Debug build, static libc++, deployment target macOS 10.14 |
| `ci-release-lto` | Host architecture | `ON` | `ON` | Host compiler | Generic release with full LTO |

```bash
# Example: Apple Silicon release with LLVM Clang (the preset configures the toolchain)
cmake --preset ci-macos-arm64
cmake --build --preset ci-macos-arm64 -j$(sysctl -n hw.ncpu)
```

## 5. Toolchains and Static libc++ (macOS 10.14+ / 11.0+ Compatibility)

To support macOS versions older than the build host (macOS 10.14+ on Intel, macOS 11.0+ on Apple Silicon), NZBGet provides a custom LLVM Clang toolchain and static runtime builder.

1. **Build static libc++ / libc++abi**:
   ```bash
   bash toolchains/macos/build-libcxx.sh
   ```
   This compiles LLVM 19 libc++ and libc++abi statically for `arm64` (targeting macOS 11.0) and `x86_64` (targeting macOS 10.14) into `build/toolchains/macos12-libcxx` (or a custom path passed as the second argument, e.g. `/opt/macos12-libcxx`).
2. **Toolchain integration**: `toolchains/macos/macos-llvm.cmake` discovers LLVM Clang from Homebrew (`/opt/homebrew/opt/llvm` or `/usr/local/opt/llvm`) and links against the static libc++ runtime:
   - `-nostdlib++`
   - statically linked `libc++.a` and `libc++abi.a`
   - explicit deployment target: `-mmacosx-version-min=10.14` (x86_64), `-mmacosx-version-min=11.0` (arm64)

## 6. Packaging `NZBGet.app` and Distribution

macOS distributions package NZBGet as a Cocoa status bar application (`NZBGet.app`) that bundles the command-line daemon, the external unpackers (`7za`, `unrar`), WebUI assets and SSL certificates.

```bash
bash platforms/macos/build.sh [bin|app|package|all] [x64|arm64|universal] [release|debug] [testing]
```

Arguments can be passed in any order:

- **Target Type** (`bin|app|package|all`): `bin` - standalone binary distribution archive (`bin/` and `share/`) without the GUI application; `app` - macOS GUI application bundle (`NZBGet.app`) and distribution archive (requires full Xcode.app); `package` or `all` (default) - the GUI application bundle if Xcode is available, otherwise the standalone binary distribution. Multiple targets can be combined (e.g. `bin app`).
- **Architecture** (`x64|arm64|universal`): `arm64` - Apple Silicon; `x64` - Intel 64-bit; `universal` - both architectures combined into one package via `lipo`.
- **Build Configuration** (`release|debug`): `release` (default) - optimizations and stripped symbols; `debug` - debug symbols and a `-debug` archive suffix.
- **Prerelease Flag** (`testing`): optional, appends a `-testing-YYYYMMDD` version suffix.

### Application Bundle Structure

```text
NZBGet.app/
└── Contents/
    ├── Info.plist
    ├── PkgInfo
    ├── MacOS/
    │   └── NZBGet                 # Cocoa status bar GUI wrapper
    └── Resources/
        ├── mainicon.icns
        ├── statusicon.png
        └── daemon/                # Embedded daemon & runtime assets
            └── usr/local/
                ├── bin/
                │   ├── nzbget     # NZBGet daemon executable
                │   ├── 7za        # 7-Zip unpacker
                │   ├── unrar      # UnRAR unpacker
                │   └── cacert.pem # Root CA certificate bundle
                └── share/nzbget/
                    ├── nzbget.conf # Master configuration template
                    ├── doc/        # ChangeLog and license files
                    └── webui/      # WebUI frontend (HTML, JS, CSS)
```

### Official Release Distribution Packages

- **`nzbget-<version>-universal.dmg`**: notarized Apple Disk Image with a Universal 2 application bundle (`arm64` + `x86_64`) for macOS 11.0 Big Sur and newer.
- **`nzbget-<version>-bin-macos-x64.zip`**: standalone (unsigned) Intel 64-bit application package for macOS 10.14 Mojave and macOS 10.15 Catalina, where the OS loader does not support Universal 2 binaries.

## 7. macOS Architecture & Runtime Path Resolution

### Separation of GUI and Daemon

When `NZBGet.app` is launched, the Cocoa GUI process (`DaemonController.m`) determines its own bundle path dynamically:

```objc
NSString *daemonPath = [[NSBundle mainBundle] pathForResource:@"daemon" ofType:nil];
```

It then spawns the embedded `nzbget` daemon with command-line overrides for the runtime paths:

```text
-o WebDir=<bundle>/Contents/Resources/daemon/usr/local/share/nzbget/webui
-o ConfigTemplate=<bundle>/Contents/Resources/daemon/usr/local/share/nzbget/nzbget.conf
-o LockFile=~/Library/Application Support/NZBGet/nzbget.lock
```

### App Translocation & Gatekeeper Mobility

Users can move `NZBGet.app` to any directory or run it from quarantine (macOS Gatekeeper App Translocation), so `WebDir`, `ConfigTemplate` and `LockFile` are never hardcoded in `nzbget.conf`; they are injected at runtime by `DaemonController`.

### Default Directory Layout (new installations)

- **`DestDir`**: `~/Downloads/NZBGet/complete` (completed downloads, visible in Finder)
- **`InterDir`**: `~/Downloads/NZBGet/intermediate` (in-progress downloads on the same filesystem volume, for fast zero-copy moves)
- **`NzbDir`**: `~/Downloads/NZBGet/nzb` (incoming `.nzb` watch directory)
- **`MainDir`**: `~/Library/Application Support/NZBGet` (queue, cache and extensions in the standard macOS Application Support directory)
- **`LogFile`**: `~/Library/Logs/NZBGet.log` (standard macOS logging directory)

## 8. Code Signing and Apple Notarization

Official macOS builds require Developer ID code signing and Apple notarization to run without Gatekeeper warnings. The script `platforms/macos/sign/nzbget-sign.sh`:

1. Deep-signs all bundled binaries (`nzbget`, `7za`, `unrar`, `NZBGet.app`).
2. Creates an Apple Disk Image (`.dmg`) with `create-dmg`, using a customized background and an Applications symlink.
3. Signs the `.dmg` container.
4. Submits the image to the Apple Notary Service (`xcrun notarytool submit --wait`).
5. Staples the notarization ticket to the disk image (`xcrun stapler staple`).

### Headless CI Keychain Setup

On headless CI runners (e.g. GitHub Actions `macos-14`) Keychain access for `codesign` must be authorized non-interactively, otherwise GUI permission prompts hang the build:

```bash
security create-keychain -p "$KEYCHAIN_PASSWORD" build.keychain
security default-keychain -s build.keychain
security unlock-keychain -p "$KEYCHAIN_PASSWORD" build.keychain
security set-keychain-settings -lut 21600 build.keychain

# Import certificate with tool permissions
security import cert.p12 -k build.keychain -P "$CERT_PASSWORD" \
  -T /usr/bin/codesign -T /usr/bin/productsign -T /usr/bin/xcrun

# CRITICAL for headless runners: partition list prevents GUI authorization dialogs
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" build.keychain
```
