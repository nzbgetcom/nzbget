# Building NZBGet for Windows

NZBGet for Windows supports two build strategies depending on your workflow:

1. **Strategy 1: Build All Dependencies from Source (Recommended)**
   CMake automatically downloads and builds all dependencies (`OpenSSL`, `zlib`, `libxml2`, `Boost`, `par2-turbo`, `rapidyenc`) from source via **FetchContent** with static CRT (`/MT`). No package manager or pre-installed libraries required. This is the recommended approach for clean, reproducible builds matching official release artifacts.
2. **Strategy 2: Fast Local Dev with System Libraries / vcpkg**
   Reuses pre-compiled static libraries from `vcpkg` or existing system installations, compiling only NZBGet and project-specific forks (`rapidyenc`, `par2-turbo`). Ideal for fast incremental iteration without waiting for OpenSSL compilation.

---

## 1. Prerequisites

### Required for all builds:
1. **MSVC C++ Build Tools / Visual Studio 2022**:
   - Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (Community or Professional) or [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/?q=build+tools).
   - In the Visual Studio Installer, select **Desktop development with C++** and ensure:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools
     - Windows 10 or 11 SDK
2. **CMake**:
   - Version 3.25 or newer is recommended:
     ```powershell
     winget install Kitware.CMake
     # or: choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'
     ```
3. **Git**:
   - Required for cloning dependencies via FetchContent:
     ```powershell
     winget install Git.Git
     ```
4. **Ninja** (the generator used by all CMake presets):
   - Install [Ninja](https://ninja-build.org/) (it is also bundled with Visual Studio's CMake tools):
     ```powershell
     winget install Ninja-build.Ninja
     # or: choco install ninja
     ```
   - Ninja relies on the MSVC environment, so run all commands from a **Developer Command Prompt / Developer PowerShell for VS 2022** that matches the target architecture (or call `vcvarsall.bat x64|x86` first).
   - A different generator can still be selected with `-G` (e.g. `-G "Visual Studio 17 2022"`), but only Ninja is covered by CI.

### Required for Strategy 1 (Building OpenSSL from source):
OpenSSL 3.x uses its own Configure script on Windows, which requires Perl and an assembler:
1. **Perl**:
   - Install [Strawberry Perl](https://strawberryperl.com/):
     ```powershell
     winget install StrawberryPerl.StrawberryPerl
     # or: choco install strawberryperl
     ```
2. **NASM**:
   - Install [NASM](https://www.nasm.us/) (Netwide Assembler):
     ```powershell
     winget install NASM.NASM
     # or: choco install nasm
     ```
   - Make sure `nasm.exe` is in your `PATH` (typically `C:\Program Files\NASM`).

### Required for Strategy 2 (Using vcpkg):
1. [vcpkg](https://github.com/microsoft/vcpkg) installed and registered (`VCPKG_ROOT` environment variable or global integration).
2. Required static libraries:
   ```powershell
   vcpkg install openssl:x64-windows-static zlib:x64-windows-static libxml2:x64-windows-static boost-json:x64-windows-static boost-test:x64-windows-static
   ```

---

## 2. Strategy 1: Build All Dependencies from Source (Recommended)

This strategy builds the entire dependency chain from source with identical compiler flags and static CRT (`/MT` in Release, `/MTd` in Debug). This is the recommended approach: it is completely self-contained and guarantees reproducible builds matching official release artifacts without needing manual package configuration.

The target architecture is selected by the MSVC environment: open a **Developer Command Prompt for VS 2022** (x64 or x86 Native Tools) in the repository root. To ensure a fully reproducible build, the CI presets enable `BUILD_DEPS_FROM_SOURCE=ON`.

- **64-bit Release** (x64 Native Tools prompt):
  ```powershell
  cmake --preset ci-windows-x64
  cmake --build --preset ci-windows-x64
  ```
  Binary location: `build/ci-windows-x64/nzbget.exe`

- **64-bit Debug (with unit tests)**:
  ```powershell
  cmake --preset debug-tests -DBUILD_DEPS_FROM_SOURCE=ON
  cmake --build --preset debug-tests
  ctest --preset debug-tests --output-on-failure
  ```

- **32-bit (x86) Release** (x86 Native Tools prompt):
  ```powershell
  cmake --preset ci-windows-x86
  cmake --build --preset ci-windows-x86
  ```
  Binary location: `build/ci-windows-x86/nzbget.exe`

> **Note on Dependency Cache**:
> All FetchContent dependencies are built and cached under `build/<config>/deps`. On CI, this directory is cached across runs.

---

## 3. Strategy 2: Fast Local Dev with vcpkg / Pre-installed Libraries

To avoid building `OpenSSL`, `libxml2`, `zlib`, and `Boost` from source every time, use pre-built static libraries with `-DBUILD_DEPS_FROM_SOURCE=OFF`.

1. Install static dependencies with vcpkg:
   ```powershell
   vcpkg install `
     openssl:x64-windows-static `
     zlib:x64-windows-static `
     libxml2:x64-windows-static `
     boost-json:x64-windows-static `
     boost-test:x64-windows-static
   ```

2. Configure CMake with the vcpkg toolchain:
   ```powershell
   cmake --preset release `
     -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
     -DVCPKG_TARGET_TRIPLET=x64-windows-static `
     -DBUILD_DEPS_FROM_SOURCE=OFF

   cmake --build --preset release
   ```

CMake will locate `OpenSSL`, `ZLIB`, `LibXml2`, and `Boost` from vcpkg via `find_package()`, while still building project forks (`rapidyenc` and `par2-turbo`) from source via FetchContent.

### Developer Presets Behavior:
The standard developer presets (`debug`, `debug-tests`, `release`, `release-lto`) set `"BUILD_DEPS_FROM_SOURCE": "OFF"`.
- If vcpkg is integrated globally (`vcpkg integrate install`), running `cmake --preset release` automatically uses the vcpkg libraries.
- If no system libraries are detected, CMake **automatically falls back to FetchContent** and builds missing dependencies from source.

---

## 4. Complete CMake Presets Reference

Defined in `CMakePresets.json`:

All presets use the **Ninja** generator (inherited from the hidden `base` preset) and must be run from a Visual Studio Developer Command Prompt that matches the target architecture.

#### Developer Presets (Daily Local Development)
Optimized for rapid local developer iteration (LTO disabled for fast linking, uses pre-installed libraries or falls back to FetchContent):

| Preset | Config | LTO | Tests | Dependencies | Description |
|---|---|---|---|---|---|
| `release` | Release | OFF | OFF | Auto / Fallback | Fast optimized release build (`/O2`, `/MT`) |
| `debug` | Debug | OFF | OFF | Auto / Fallback | Debug build with `/MTd` static CRT |
| `debug-tests` | Debug | OFF | **ON** | Auto / Fallback | Debug build with unit test suite (`nzbget_tests`) |
| `debug-asan` | Debug | OFF | **ON** | Auto / Fallback | Address + Undefined sanitizers |
| `debug-tsan` | Debug | OFF | **ON** | Auto / Fallback | Thread sanitizer |
| `reldebinfo` | RelWithDebInfo | OFF | OFF | Auto / Fallback | Release build with `.pdb` symbols for profiling |
| `minsizerel` | MinSizeRel | OFF | OFF | Auto / Fallback | Size-optimized build (`/O1`, `/MT`) |
| `release-lto` | Release | **ON** | OFF | Auto / Fallback | Release build with Link-Time Optimization |

#### CI Automation Presets (Continuous Integration)
Configured specifically for GitHub Actions runners to produce identical official release binaries:

| Preset | Environment | Config | LTO | Tests | Dependencies | Description |
|---|---|---|---|---|---|---|
| `ci-windows-x64` | x64 Native Tools | Release | **ON** | OFF | **FetchContent (Force)** | Official 64-bit CI release build (`build/ci-windows-x64`) |
| `ci-windows-x86` | x86 Native Tools | Release | **ON** | OFF | **FetchContent (Force)** | Official 32-bit CI release build (`build/ci-windows-x86`) |
| `ci-release-lto` | Native | Release | **ON** | OFF | **FetchContent (Force)** | Universal CI release preset with LTO |

> **Understanding CI Presets & Reproducing CI Locally**:
> The `ci-*` presets are tailored for automated pipelines:
> 1. They enforce `BUILD_DEPS_FROM_SOURCE=ON` to guarantee hermetic, clean dependency building.
> 2. They enable Link-Time Optimization (`ENABLE_LTO=ON`) for maximum binary runtime performance (which takes longer to link).
> 3. They output to isolated directories (`build/ci-windows-x64`) matching CI runner cache keys.
> 
> If a CI build fails or you need to inspect the exact binary output produced by the GitHub Actions runner, you can execute:
> ```powershell
> cmake --preset ci-windows-x64
> cmake --build --preset ci-windows-x64
> ```

---

## 5. CMake Options Reference

Customize your build with `-D<OPTION>=<VALUE>`:

| Option | Default | Description |
|---|---|---|
| `BUILD_DEPS_FROM_SOURCE` | `OFF` | `ON` forces building all dependencies from source via FetchContent (guaranteeing hermetic builds; default in `build.ps1` and CI presets). `OFF` (CMake default) checks `find_package()` first with automatic fallback to FetchContent. |
| `DISABLE_PARCHECK` | `OFF` | Build without par2-turbo repair support |
| `DISABLE_GZIP` | `OFF` | Build without zlib compression support |
| `ENABLE_TESTS` | `OFF` | Build unit tests target (`nzbget_tests`) |
| `ENABLE_LTO` | `OFF` | Enable Link-Time Optimization (`/GL`, `/LTCG`) |
| `USE_SANITIZERS` | `""` | Enable sanitizers, e.g. `-DUSE_SANITIZERS=address` |

---

## 6. Automated Packaging & Installer Script

To produce official release packages (64-bit and 32-bit executables, debug symbols, and the NSIS setup installer):

```powershell
.\platforms\windows\build.ps1 -BuildRelease -Build32 -Build64 -BuildSetup
```

See [platforms/windows/build-info.md](../platforms/windows/build-info.md) for full details on packaging requirements (NSIS plugins, unpackers).
