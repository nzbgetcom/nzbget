# Building NZBGet for Apple Platforms (macOS & Apple OS)

This document provides a comprehensive guide for configuring, building, testing, and packaging NZBGet for Apple platforms, primarily macOS (Apple Silicon `arm64` and Intel `x86_64`), as well as architectural notes for future Apple platform support (iOS / iPadOS).

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
3. **Ninja** (recommended for fast parallel builds):
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
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (default in dev presets) | Tries to find system/Homebrew libraries first (`FindOpenSSL`, `FindZLIB`, `FindLibXml2`, `FindBoost`). If not found, falls back automatically to FetchContent. | Local development, rapid iteration |
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

#### Multi-Config Generator (Ninja Multi-Config or Xcode)

- **Debug build**:
  ```bash
  cmake --preset debug
  cmake --build --preset debug
  ```
  Binary location: `build/debug/Debug/nzbget`

- **Release build**:
  ```bash
  cmake --preset release
  cmake --build --preset release
  ```
  Binary location: `build/release/Release/nzbget`

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

#### Single-Config Ninja Generator (Fast Incremental Builds)

- **Ninja Debug**:
  ```bash
  cmake --preset ninja-debug
  cmake --build --preset ninja-debug
  ```
  Binary location: `build/ninja-debug/nzbget`

- **Ninja Release**:
  ```bash
  cmake --preset ninja-release
  cmake --build --preset ninja-release
  ```
  Binary location: `build/ninja-release/nzbget`

---

## 4. Cross-Compilation and CI Presets

For cross-compiling or building standalone redistributable binaries, dedicated CI presets are provided:

| Preset Name | Target Architecture | Source Deps | LTO | Compiler | Notes |
|---|---|---|---|---|---|
| `ci-macos-arm64` | `arm64` (Apple Silicon) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 12.0 |
| `ci-macos-x64` | `x86_64` (Intel Mac) | `ON` | `ON` | LLVM Clang 19 | Static libc++, deployment target macOS 12.0 |
| `ci-release-lto` | Host architecture | `ON` | `ON` | Host compiler | Generic release with full LTO |

### Building with a CI Preset
```bash
# Example: Build Apple Silicon release with LLVM Clang
export CMAKE_TOOLCHAIN_FILE=toolchains/macos-llvm.cmake
cmake --preset ci-macos-arm64
cmake --build --preset ci-macos-arm64 -j$(sysctl -n hw.ncpu)
```

---

## 5. Toolchains and Static libc++ (macOS 12+ Compatibility)

To support macOS versions older than the host runner (targeting macOS 12.0+), NZBGet provides a custom LLVM Clang toolchain and static runtime builder.

### Static libc++ Workflow
1. **Build Static libc++ / libc++abi**:
   Run the runtime builder script:
   ```bash
   bash toolchains/build-macos-libcxx.sh
   ```
   This compiles LLVM 19 libc++ and libc++abi statically for both `arm64` and `x86_64`, deploying to `/opt/macos12-libcxx` (or `${LIBCXX_PREFIX}`).
2. **Toolchain Integration**:
   `toolchains/macos-llvm.cmake` automatically discovers LLVM Clang from Homebrew (`/opt/homebrew/opt/llvm` or `/usr/local/opt/llvm`) and links against the static libc++ runtime:
   - `-nostdlib++`
   - Statically linked `libc++.a` and `libc++abi.a`
   - Explicit deployment target `-mmacosx-version-min=12.0`

---

## 6. Packaging `NZBGet.app` and Distribution

macOS distributions package NZBGet as a Cocoa status bar application (`NZBGet.app`) that bundles the command-line daemon, external unpackers (`7za`, `unrar`), WebUI assets, and SSL certificates.

### Build Script (`platforms/macos/build.sh`)

The script `platforms/macos/build.sh` automates compiling, bundling, and packaging:

```bash
bash platforms/macos/build.sh [arch] [testing]
```

- **`arch`**:
  - `arm64` — Apple Silicon package
  - `x64` — Intel package
  - `universal` (default) — Builds both architectures and merges binaries with `lipo`
- **`testing`**:
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

---

## 9. Future Apple Platforms (iOS / iPadOS)

NZBGet's modular POSIX/CMake foundation enables future targeting of iOS and iPadOS. Key architectural considerations:

1. **Static Linking & Frameworks**:
   iOS requires all code and non-system libraries to be statically linked or packaged inside the application bundle. FetchContent static libraries (`BUILD_DEPS_FROM_SOURCE=ON`) natively satisfy this requirement.
2. **Background Execution**:
   Unlike macOS where NZBGet can run as a persistent background daemon, iOS enforces strict app lifecycle suspended states. NZBGet on iOS would require:
   - `BGProcessingTask` / background URL sessions for active downloads.
   - Foreground execution when high-throughput PAR2 repair or unrar unpacking occurs.
3. **Sandbox Storage**:
   Filesystem access on iOS is strictly isolated to the app's `Documents` and `Library/Caches` containers. Path configuration (`MainDir`, `DestDir`, `InterDir`) must dynamically default to `NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)`.
4. **Toolchain Cross-Compilation**:
   Building for iOS can be achieved using CMake's built-in iOS support:
   ```bash
   cmake -B build-ios -G Xcode \
     -DCMAKE_SYSTEM_NAME=iOS \
     -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
     -DBUILD_DEPS_FROM_SOURCE=ON \
     -DENABLE_PAR2_TURBO=ON
   ```
