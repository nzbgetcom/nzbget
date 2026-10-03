# Building NZBGet Windows Binaries and Installer

`platforms\windows\build.ps1` is the PowerShell script used to build Windows nzbget binaries and the NSIS installer. It automates multi-architecture (32/64-bit) builds, release packaging, and NSIS installer generation.

---

## 1. Prerequisites

### PowerShell Execution Policy
Ensure your system allows executing PowerShell scripts. In a PowerShell prompt:
```powershell
Get-ExecutionPolicy
```
It should be at least `RemoteSigned`. If not, run as Administrator:
```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

### Build Tools & Compilers
- **MSVC v143** (Visual Studio 2022 or Build Tools for Visual Studio 2022) with C++ Desktop Workload.
- **CMake** (>= 3.25) in `PATH`.
- **NASM** (for OpenSSL assembly optimization) in `PATH` (`choco install nasm`).
- **Strawberry Perl** (for OpenSSL configuration) in `PATH` (`choco install strawberryperl`).
- **Optional: jom** (for parallel OpenSSL compilation) in `PATH` (`choco install jom`).

> **Note on Dependencies:** All C++ dependencies (`OpenSSL`, `zlib`, `libxml2`, `Boost`, `par2-turbo`, `rapidyenc`) are managed hermetically via CMake `FetchContent`. No external package manager (Conan or vcpkg) is required.

### Installer Packaging Requirements (for `-BuildSetup`)
- **NSIS 3.09** or newer installed in `C:\nzbget\nsis` (or extracted via tools archive).
  - [Advanced logging](https://nsis.sourceforge.io/Special_Builds) build
  - [AccessControl Plugin](https://nsis.sourceforge.io/AccessControl_plug-in) (`AccessControl.dll` in NSIS `Plugins\x86-unicode`)
  - [Simple Service Plugin](https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin) (`SimpleSC.dll` in NSIS `Plugins\x86-unicode`)
- **Unpackers**:
  Download 7za and unrar unpackers to `$ToolsRoot\image` (`C:\nzbget\image\32` and `C:\nzbget\image\64`) using:
  ```powershell
  .\platforms\windows\build.ps1 -DownloadUnpackers
  ```

In GitHub Actions CI, these packaging tools are downloaded automatically as a pre-configured bundle:
`https://github.com/nzbgetcom/build-files/releases/download/v10.0/nzbget-windows-tools.zip` and unpacked to `C:\`.

---

## 2. Running the Build Script

Run from the root of the repository in PowerShell:

```powershell
.\platforms\windows\build.ps1 [options]
```

### Available Options

| Option | Description |
|---|---|
| `-BuildRelease` | Build Release binaries (with `/MT` static CRT, LTO enabled by default) |
| `-BuildDebug` | Build Debug binaries (with `/MTd` debug CRT and tests enabled) |
| `-Build32` | Build 32-bit (x86) binaries |
| `-Build64` | Build 64-bit (x64) binaries |
| `-BuildSetup` | Build NSIS setup installer package (`nzbget-*-windows-setup.exe`) |
| `-BuildTesting` | Append testing VersionSuffix (`-testing-YYYYMMDD`) to package version |
| `-DownloadUnpackers` | Download 7za and unrar unpackers into `$ToolsRoot\image` |

### Common Examples

1. **Build 64-bit Release binary**:
   ```powershell
   .\platforms\windows\build.ps1 -BuildRelease -Build64
   ```

2. **Build 64-bit and 32-bit Release binaries + NSIS installer**:
   ```powershell
   .\platforms\windows\build.ps1 -BuildRelease -Build32 -Build64 -BuildSetup
   ```

3. **Build both Release and Debug installers with testing date suffix**:
   ```powershell
   .\platforms\windows\build.ps1 -BuildRelease -BuildDebug -Build32 -Build64 -BuildSetup -BuildTesting
   ```

---

## 3. Output Layout

| Path | Description |
|---|---|
| `build\Release64\Release\nzbget.exe` | 64-bit Release executable |
| `build\Release32\Release\nzbget.exe` | 32-bit Release executable |
| `build\Debug64\Debug\nzbget.exe` | 64-bit Debug executable |
| `build\Debug32\Debug\nzbget.exe` | 32-bit Debug executable |
| `build\distrib\nzbget\` | Prepared staging directory for packaging |
| `build\nzbget-<version>-bin-windows-setup.exe` | Final NSIS setup installer |
