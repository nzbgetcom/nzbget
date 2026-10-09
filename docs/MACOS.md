# Building NZBGet for macOS

This document provides a comprehensive guide for configuring, building, testing, and packaging NZBGet for macOS (Apple Silicon `arm64` and Intel `x86_64`).

NZBGet uses modern **CMake** (3.25+) with **FetchContent** to manage dependencies cleanly and deterministically.

---

## 1. Prerequisites

### Basic Tools
1. **Xcode Command Line Tools** (or full Xcode):
   ```bash
   xcode-select --install
   ```
2. **CMake** (3.25 or newer):
   ```bash
   brew install cmake
   ```
3. **Ninja** (the generator used by all CMake presets; override with `-G` if needed):
   ```bash
   brew install ninja
   ```

### Optional Dependencies & Tools
- **Homebrew packages** (for local development using system libraries):
  ```bash
  brew install openssl@3 zlib libxml2 boost
  ```
- **Packaging & Notarization Tools** (for building `.dmg` packages):
  ```bash
  brew install create-dmg
  ```
- **LLVM Toolchain** (used in release CI for hermetic builds with static libc++):
  ```bash
  brew install llvm@19
  ```

---

## 2. Dependency Management: Source vs System

NZBGet supports two dependency resolution models controlled by the CMake cache variable `BUILD_DEPS_FROM_SOURCE`:

| Option | Value | Behavior | Recommended For |
|---|---|---|---|
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (default in dev presets) | Tries to find system/Homebrew libraries first (`FindOpenSSL`, `FindZLIB`, `FindLibXml2`, `FindBoost`). If not found, falls back automatically to FetchContent. `rapidyenc` and `par2-turbo` are always built from source via FetchContent. | Local development, rapid iteration |
| `BUILD_DEPS_FROM_SOURCE` | `ON` (default in `ci-*` presets) | Pure hermetic build: fetches and compiles all dependencies from source via CMake FetchContent. | CI, release builds, redistributable binaries |

### Dependencies Managed via FetchContent
- **OpenSSL**: Built from source via `jimmy-park/openssl-cmake` (3.x static library).
- **zlib**: Built from `madler/zlib` (static library).
- **libxml2**: Built from GNOME GitLab (configured without Python/tests, static library).
- **Boost**: Header-only integration (Boost.JSON, Boost.Asio).
- **par2-turbo**: High-performance PAR2 verification and repair (static library).
- **rapidyenc**: High-performance SIMD-accelerated yEnc decoding (static library).

---

## 3. Quick Start: Using CMake Presets

NZBGet defines presets in `CMakePresets.json` for standard developer workflows.

### Local Development (Apple Clang / Default Compiler)

All presets use the **Ninja** generator. To use another generator (such as Xcode or Unix Makefiles), pass `-G <generator>` to the `cmake` invocation.

- **Debug build**:
  ```bash
  cmake --preset debug
  cmake --build --preset debug
  ```
  Binary location: `build/debug/nzbget`

- **Release build**:
  ```bash
  cmake --preset release
  cmake --build --preset release
  ```
  Binary location: `build/release/nzbget`

- **Run unit tests**:
  ```bash
  cmake --preset debug-tests
  cmake --build --preset debug-tests
  ctest --preset debug-tests --output-on-failure
  ```

- **Sanitizer builds** (detect memory bugs and concurrency issues):
  ```bash
  # AddressSanitizer + UndefinedBehaviorSanitizer:
  cmake --preset debug-asan && cmake --build --preset debug-asan

  # ThreadSanitizer:
  cmake --preset debug-tsan && cmake --build --preset debug-tsan
  ```

---

## 4. Cross-Compilation and CI Presets

For cross-compiling or building standalone redistributable binaries, dedicated CI presets are provided:

| Preset Name | Target Architecture | Source Deps | LTO | Compiler | Notes |
|---|---|---|---|---|---|
| `ci-macos-arm64` | `arm64` (Apple Silicon) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 11.0 (Big Sur), curses disabled |
| `ci-macos-x64` | `x86_64` (Intel Mac) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 10.14 (Mojave), curses disabled |
| `ci-macos-debug-arm64` | `arm64` (Apple Silicon) | `ON` | `OFF` | LLVM Clang 19 | Debug build, static libc++, deployment target macOS 11.0 |
| `ci-macos-debug-x64` | `x86_64` (Intel Mac) | `ON` | `OFF` | LLVM Clang 19 | Debug build, static libc++, deployment target macOS 10.14 |
| `ci-release-lto` | Host architecture | `ON` | `ON` | Host compiler | Generic release with full LTO |

### Building with a CI Preset
```bash
# Example: Build Apple Silicon release with LLVM Clang (toolchain configured automatically by preset)
cmake --preset ci-macos-arm64
cmake --build --preset ci-macos-arm64 -j$(sysctl -n hw.ncpu)
```

---

## 5. Toolchains and Static libc++ (macOS 10.14+ / 11.0+ Compatibility)

To support macOS versions older than the host runner (targeting macOS 10.14+ on Intel and macOS 11.0+ on Apple Silicon), NZBGet provides a custom LLVM Clang toolchain and static runtime builder.

### Static libc++ Workflow
1. **Build Static libc++ / libc++abi**:
   Run the runtime builder script:
   ```bash
   bash toolchains/build-macos-libcxx.sh
   ```
   This compiles LLVM 19 libc++ and libc++abi statically for both `arm64` (targeting macOS 11.0) and `x86_64` (targeting macOS 10.14), deploying to `build/toolchains/macos12-libcxx` (or a custom path passed as the second argument, e.g. `/opt/macos12-libcxx`).
2. **Toolchain Integration**:
   `toolchains/macos-llvm.cmake` automatically discovers LLVM Clang from Homebrew (`/opt/homebrew/opt/llvm` or `/usr/local/opt/llvm`) and links against the static libc++ runtime:
   - `-nostdlib++`
   - Statically linked `libc++.a` and `libc++abi.a`
   - Explicit deployment target (`-mmacosx-version-min=10.14` for x86_64, `-mmacosx-version-min=11.0` for arm64)

---

## 6. Packaging `NZBGet.app` and Distribution

macOS distributions package NZBGet as a Cocoa status bar application (`NZBGet.app`) that bundles the command-line daemon, external unpackers (`7za`, `unrar`), WebUI assets, and SSL certificates.

### Build Script (`platforms/macos/build.sh`)

The script `platforms/macos/build.sh` automates compiling, bundling, and packaging:

```bash
bash platforms/macos/build.sh [bin|app|package|all] [x64|arm64|universal] [release|debug] [testing]
```

Arguments can be passed in any order:
- **Target Type** (`bin|app|package|all`):
  - `bin` — standalone binary distribution archive (`bin/` and `share/`) without building GUI application.
  - `app` — macOS GUI application bundle (`NZBGet.app`) and distribution archive (requires full Xcode.app).
  - `package` or `all` (default) — builds the GUI application bundle if Xcode is available, falling back to standalone binary distribution. Multiple targets can be specified together (e.g. `bin app` packages both).
- **Architecture** (`x64|arm64|universal`):
  - `arm64` — Apple Silicon package.
  - `x64` — Intel 64-bit package.
  - `universal` — Builds both architectures and creates a universal binary package via `lipo`.
- **Build Configuration** (`release|debug`):
  - `release` (default) — Release build with optimizations and stripped symbols.
  - `debug` — Debug build with debug symbols (`-debug` archive suffix).
- **Prerelease Flag** (`testing`):
  - Optional flag. Appends `-testing-YYYYMMDD` version suffix for prerelease builds.

### Application Bundle Structure
The resulting `.app` bundle follows standard Apple bundle conventions:
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

NZBGet publishes two standard macOS packages for releases:
- **`nzbget-<version>-universal.dmg`**: Notarized Apple Disk Image containing a Universal 2 application bundle (`arm64` + `x86_64`) for macOS 11.0 Big Sur and newer (Apple Silicon and modern Intel Macs).
- **`nzbget-<version>-bin-macos-x64.zip`**: Standalone (unsigned) Intel 64-bit application package for macOS 10.14 Mojave and macOS 10.15 Catalina users, where Universal 2 binaries are not supported by the OS loader.

---

## 7. macOS Architecture & Runtime Path Resolution

### Separation of GUI and Daemon
- **Cocoa Wrapper (`DaemonController.m`)**:
  When `NZBGet.app` is launched, the Cocoa GUI process determines its own bundle path dynamically:
  ```objc
  NSString *daemonPath = [[NSBundle mainBundle] pathForResource:@"daemon" ofType:nil];
  ```
  It then spawns the embedded `nzbget` daemon passing command-line overrides for runtime paths:
  ```text
  -o WebDir=<bundle>/Contents/Resources/daemon/usr/local/share/nzbget/webui
  -o ConfigTemplate=<bundle>/Contents/Resources/daemon/usr/local/share/nzbget/nzbget.conf
  -o LockFile=~/Library/Application Support/NZBGet/nzbget.lock
  ```

### App Translocation & Gatekeeper Mobility
Because users can move `NZBGet.app` to any directory or run it from quarantine (macOS Gatekeeper App Translocation), paths to `WebDir`, `ConfigTemplate`, and `LockFile` are **never hardcoded** in `nzbget.conf`. They are dynamically injected at runtime by `DaemonController`.

### Default Directory Layout (for new installations)
On macOS, user-facing download folders and internal application state are separated cleanly:
- **`DestDir`**: `~/Downloads/NZBGet/complete` (completed downloads, visible in Finder)
- **`InterDir`**: `~/Downloads/NZBGet/intermediate` (in-progress downloads on the same filesystem volume for fast zero-copy move)
- **`NzbDir`**: `~/Downloads/NZBGet/nzb` (incoming `.nzb` watch directory)
- **`MainDir`**: `~/Library/Application Support/NZBGet` (queue, cache, and extensions in standard macOS Application Support)
- **`LogFile`**: `~/Library/Logs/NZBGet.log` (standard macOS logging directory)

---

## 8. Code Signing and Apple Notarization

Official macOS builds require Developer ID code signing and Apple notarization to execute without Gatekeeper warnings.

### Signing Script (`platforms/macos/sign/nzbget-sign.sh`)
The signing script handles:
1. Deep code signing of all bundled binaries (`nzbget`, `7za`, `unrar`, `NZBGet.app`).
2. Creating an Apple Disk Image (`.dmg`) using `create-dmg` with customized background and Applications symlink.
3. Signing the `.dmg` container.
4. Submitting to Apple Notary Service via `xcrun notarytool submit --wait`.
5. Stapling the notarization ticket to the disk image using `xcrun stapler staple`.

### Headless CI Keychain Setup
On headless CI runners (e.g. GitHub Actions `macos-14`), Keychain access for `codesign` must be authorized non-interactively to avoid GUI permission prompts that hang the build:
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
