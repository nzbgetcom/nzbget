# macOS Build & Packaging Information

This directory contains the macOS platform-specific assets, build automation, Cocoa wrapper application, and code signing utilities for NZBGet.

For comprehensive build instructions, CMake presets, toolchain details, and dependency options, see the primary documentation:
👉 **[Apple Platform Guide (docs/APPLE.md)](../../docs/APPLE.md)**

---

## Quick Reference

### Directory Layout
- `build.sh`: Main shell script for compiling per-architecture binaries, universal binaries (`lipo`), assembling the `.app` bundle, and packaging zip/DMG artifacts.
- `NZBGet/`: Xcode project source files for the native macOS Cocoa menu bar / status bar wrapper application (`NZBGet.app`).
  - `DaemonController.m`: Spawns and manages the embedded `nzbget` daemon, dynamically supplying runtime paths (`WebDir`, `ConfigTemplate`, `LockFile`).
  - `MainController.m`: Status bar menu, Dock icon controls, and WebUI launcher.
  - `PreferencesController.m`: GUI Preferences dialog for basic settings.
- `Resources/`: Icons (`mainicon.icns`, `statusicon.png`), localized strings, and daemon resource staging area.
- `sign/`: Scripts and entitlements for macOS Developer ID code signing, DMG creation, and Apple Notarization (`nzbget-sign.sh`).

### Building Packages with `build.sh`

```bash
# Build Universal (x86_64 + arm64) package (default):
bash platforms/macos/build.sh

# Build Apple Silicon only (arm64):
bash platforms/macos/build.sh arm64

# Build Intel only (x64):
bash platforms/macos/build.sh x64

# Build prerelease testing package (adds -testing-YYYYMMDD suffix):
bash platforms/macos/build.sh universal testing
```

All build artifacts are written to `build/dist/` in the repository root.
