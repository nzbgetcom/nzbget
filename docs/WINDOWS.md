# Building NZBGet for Windows

NZBGet uses CMake with **FetchContent** to automatically download and build all dependencies (`OpenSSL`, `zlib`, `libxml2`, `Boost`, `par2-turbo`, `rapidyenc`) hermetically from source.

---

## 1. Prerequisites

1. **MSVC C++ Build Tools / Visual Studio 2022**:
   - Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (Community or Professional) or [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/?q=build+tools).
   - In the Visual Studio Installer, select **Desktop development with C++** and ensure the following components are selected:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools
     - Windows 10 or 11 SDK

2. **CMake**:
   - Version 3.25 or newer is recommended. Download from [cmake.org](https://cmake.org/download/) or install via:
     ```powershell
     winget install Kitware.CMake
     # or: choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'
     ```

3. **Perl** (required for building OpenSSL from source):
   - Install [Strawberry Perl](https://strawberryperl.com/):
     ```powershell
     winget install StrawberryPerl.StrawberryPerl
     # or: choco install strawberryperl
     ```

4. **NASM** (required for building OpenSSL assembly):
   - Install [NASM](https://www.nasm.us/):
     ```powershell
     winget install NASM.NASM
     # or: choco install nasm
     ```
   - Make sure `nasm.exe` is in your `PATH` (typically `C:\Program Files\NASM`).

5. **Optional: jom** (for parallel OpenSSL compilation):
   - Multi-threaded tool for NMake files:
     ```powershell
     choco install jom
     ```

6. **Optional: Ninja** (for faster incremental builds):
   - Install [Ninja](https://ninja-build.org/):
     ```powershell
     winget install Ninja-build.Ninja
     # or: choco install ninja
     ```

---

## 2. Quick Start: Using CMake Presets

NZBGet provides pre-configured CMake presets for Visual Studio and Ninja.

Open a PowerShell or Developer Command Prompt in the NZBGet repository:

### Visual Studio Multi-Config Generator (Default)

- **Debug build**:
  ```powershell
  cmake --preset debug
  cmake --build --preset debug
  ```
  Binary location: `build/debug/Debug/nzbget.exe`

- **Release build** (optimized, static runtime `/MT`):
  ```powershell
  cmake --preset release
  cmake --build --preset release
  ```
  Binary location: `build/release/Release/nzbget.exe`

- **Release with LTO** (Link-Time Optimization):
  ```powershell
  cmake --preset release-lto
  cmake --build --preset release-lto
  ```
  Binary location: `build/release-lto/Release/nzbget.exe`

- **Run unit tests**:
  ```powershell
  cmake --preset debug-tests
  cmake --build --preset debug-tests
  ctest --preset debug-tests
  ```

### Ninja Single-Config Generator (Fast Parallel Builds)

- **Ninja Debug**:
  ```powershell
  cmake --preset ninja-debug
  cmake --build --preset ninja-debug
  ```
  Binary location: `build/ninja-debug/nzbget.exe`

- **Ninja Release**:
  ```powershell
  cmake --preset ninja-release
  cmake --build --preset ninja-release
  ```
  Binary location: `build/ninja-release/nzbget.exe`

- **Ninja Release with LTO**:
  ```powershell
  cmake --preset ninja-release-lto
  cmake --build --preset ninja-release-lto
  ```
  Binary location: `build/ninja-release-lto/nzbget.exe`

- **Ninja Debug with Tests**:
  ```powershell
  cmake --preset ninja-debug-tests
  cmake --build --preset ninja-debug-tests
  ctest --preset ninja-debug-tests
  ```

### Complete Presets Reference Table

All presets defined in `CMakePresets.json`:

#### Visual Studio Generator (Multi-Config, Default on Windows)
| Preset | Configuration | LTO | Tests | Source Deps | Description |
|---|---|---|---|---|---|
| `debug` | Debug | OFF | OFF | System / Auto | Debug build with `/MTd` static CRT |
| `debug-tests` | Debug | OFF | ON | System / Auto | Debug build with unit test targets |
| `debug-asan` | Debug | OFF | ON | System / Auto | Address + Undefined sanitizers |
| `debug-tsan` | Debug | OFF | ON | System / Auto | Thread sanitizer |
| `release` | Release | OFF | OFF | System / Auto | Optimized release build (`/O2`, `/MT`) |
| `reldebinfo` | RelWithDebInfo | OFF | OFF | System / Auto | Optimized build with debug symbols (`.pdb`) |
| `minsizerel` | MinSizeRel | OFF | OFF | System / Auto | Size-optimized build (`/O1`, `/MT`) |
| `release-lto` | Release | ON | OFF | System / Auto | Release with Link-Time Optimization (`/GL`, `/LTCG`) |
| `ci-release-lto` | Release | ON | OFF | ON | Hermetic CI build from source with LTO |
| `ci-windows-x64` | Release | ON | OFF | ON | 64-bit CI packaging build |
| `ci-windows-x86` | Release | ON | OFF | ON | 32-bit CI packaging build |

#### Ninja Generator (Single-Config, Fast Parallel Builds)
| Preset | Configuration | LTO | Tests | Source Deps | Description |
|---|---|---|---|---|---|
| `ninja-debug` | Debug | OFF | OFF | System / Auto | Fast debug build via Ninja |
| `ninja-release` | Release | OFF | OFF | System / Auto | Fast release build via Ninja |
| `ninja-release-lto` | Release | ON | OFF | System / Auto | Fast release build with LTO via Ninja |
| `ninja-debug-tests` | Debug | OFF | ON | System / Auto | Fast debug build with unit tests via Ninja |

### Overriding Preset Variables via Command Line

Command-line `-D` options always take precedence over preset defaults:

```powershell
# Override: debug build with hermetic dependency build from source
cmake --preset debug -DBUILD_DEPS_FROM_SOURCE=ON
cmake --build --preset debug

# Override: build without TLS (no Perl / NASM required)
cmake --preset debug -DDISABLE_TLS=ON
cmake --build --preset debug

# Override: build in a custom directory
cmake --preset debug -B build/custom-debug -DBUILD_DEPS_FROM_SOURCE=ON
cmake --build build/custom-debug --config Debug
```

---

## 3. Manual CMake Configuration

If you prefer configuring CMake manually without presets:

### 64-bit (x64) Release
```powershell
cmake -B build/x64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/x64 --config Release
```

### 32-bit (x86) Release
```powershell
cmake -B build/x86 -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build/x86 --config Release
```

---

## 4. CMake Options

You can customize the build with standard `-D<OPTION>=<VALUE>` flags:

| Option | Default | Description |
|---|---|---|
| `DISABLE_TLS` | `OFF` | Build without OpenSSL / TLS support |
| `DISABLE_PARCHECK` | `OFF` | Build without par2-turbo repair support |
| `DISABLE_GZIP` | `OFF` | Build without zlib compression support |
| `ENABLE_TESTS` | `OFF` | Build unit tests target (`nzbget-tests`) |
| `ENABLE_LTO` | `OFF` | Enable Link-Time Optimization (`/GL`, `/LTCG`) |
| `BUILD_DEPS_FROM_SOURCE` | `OFF` (POSIX) / `ON` (Windows) | Force building dependencies hermetically via FetchContent |
| `USE_SANITIZERS` | `""` | Enable sanitizers, e.g. `-DUSE_SANITIZERS=address` |

---

## 5. Automated Build & Installer Script

To produce official release packages and the NSIS setup installer, use the PowerShell script:

```powershell
# Build 64-bit and 32-bit Release binaries and NSIS installer
.\platforms\windows\build.ps1 -BuildRelease -Build32 -Build64 -BuildSetup
```

See [platforms/windows/build-info.md](../platforms/windows/build-info.md) for full details on packaging requirements (NSIS plugins, unpackers).
