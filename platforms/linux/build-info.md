# Linux Build & Packaging Information

This directory contains Linux platform-specific assets, packaging configurations, and utilities for NZBGet.

For comprehensive build instructions, CMake presets, toolchain details, and dependency options, see the primary documentation:
**[Linux Platform Guide (docs/LINUX.md)](../../docs/LINUX.md)**

## Toolchain layout

`$NZBGET_LINUX_TOOLCHAIN_DIR` (default `build/toolchains/linux`, builder image `/opt/nzbget/linux`):

```text
<arch>/bin/<triple>-gcc
```

`<arch>` is `x86_64`, `aarch64`, `armhf`, `armel`, `i686`, `ppc6xx`, or `riscv64`. `<triple>` is the matching musl triplet (`x86_64-linux-musl`, `aarch64-linux-musl`, `arm-linux-musleabihf`, `arm-linux-musleabi`, `i686-linux-musl`, `powerpc-linux-musl`, `riscv64-linux-musl`).

The [nzbget-musl-builder](https://github.com/nzbgetcom/nzbget-musl-builder) images already contain this layout.

---

## Directory Layout

- `build.sh`: Platform driver script: compiles per-architecture binaries, builds binary tarballs, and packages the multi-architecture `.run` installer.
- `installer.sh`: Self-extracting POSIX shell installer header for `.run` packages. Automatically detects host CPU architecture (`x86_64`, `aarch64`, `armhf`, `armel`, `i686`, `ppc6xx`, `riscv64`) and unpacks the matching binary.
- `install-update.sh`: Self-update script invoked by NZBGet WebUI/RPC updater. Verifies package signature, updates files, and safely restarts background daemon mode.
- `package-info.json`: Descriptor defining the remote update feed link and update script name.
- `pkg/`: Debian (`.deb`) and Red Hat (`.rpm`) packaging specifications, systemd service unit (`nzbget.service`), and builder script (`build-pkg.sh`).
- `flatpak/`: Flatpak manifest (`com.nzbget.nzbget.yml`), AppStream metainfo, desktop entry, launcher, and build script.
- `snap/`: Snapcraft recipe (`snapcraft.yaml`) for Canonical Snap Store packaging.
- `qnap/`: QNAP QPKG repacking utilities and scripts (`repack-nzbget.sh`).

---

## Target Architectures

NZBGet targets 7 CPU architectures for Linux with 100% static linkage via musl libc and GCC 14.3:

| Arch Parameter | GNU / Musl Triplet | Architecture Family | Common Devices / Hardware |
|---|---|---|---|
| `x86_64` | `x86_64-linux-musl` | x86 64-bit AMD/Intel | Modern PCs, servers, NAS (Synology/QNAP/Asustor) |
| `aarch64` | `aarch64-linux-musl` | ARM 64-bit (ARMv8-A+) | Raspberry Pi 3/4/5, Apple Silicon VMs, ARM64 servers |
| `armhf` | `arm-linux-musleabihf` | ARM 32-bit Hard Float (ARMv7-A) | Raspberry Pi 2, BeagleBone, Cortex-A NAS devices |
| `armel` | `arm-linux-musleabi` | ARM 32-bit Soft Float (ARMv5TE/v6) | Legacy ARM9/ARM11 devices, older Marvell Kirkwood NAS |
| `i686` | `i686-linux-musl` | x86 32-bit Pentium Pro+ | Legacy 32-bit PCs, older Atom/Celeron NAS devices |
| `ppc6xx` | `powerpc-linux-musl` | PowerPC 32-bit (e300 / 603e) | Legacy PowerPC NAS (Synology DS209+ / DS409+, QNAP) |
| `riscv64` | `riscv64-linux-musl` | RISC-V 64-bit (RV64GC) | Modern RISC-V boards (VisionFive 2, Milk-V Mars) |
