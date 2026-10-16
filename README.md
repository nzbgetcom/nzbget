# NZBGet — Efficient Usenet downloader

[![License: GPL-2.0-or-later](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](COPYING)
[![Release](https://img.shields.io/github/v/release/nzbgetcom/nzbget?include_prereleases&sort=semver)](https://github.com/nzbgetcom/nzbget/releases)
[![Downloads](https://img.shields.io/github/downloads/nzbgetcom/nzbget/total)](https://github.com/nzbgetcom/nzbget/releases)
[![Docker pulls](https://img.shields.io/docker/pulls/nzbgetcom/nzbget.svg)](https://hub.docker.com/u/nzbgetcom)

[![tests](https://github.com/nzbgetcom/nzbget/actions/workflows/tests.yml/badge.svg?branch=develop)](https://github.com/nzbgetcom/nzbget/actions/workflows/tests.yml)
[![build](https://github.com/nzbgetcom/nzbget/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/build.yml)
[![docker build](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml)

[![GitHub issues](https://img.shields.io/github/issues/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/pulls)
[![Discord](https://img.shields.io/badge/Discord-%235865F2.svg?&logo=discord&logoColor=white)](https://discord.gg/mV9Vn9sM7C)

NZBGet is a binary downloader that downloads files from Usenet based on information given in nzb files.

NZBGet is written in C++ and is designed for performance and efficiency.

NZBGet can run on almost any device — classic PC, NAS, media player, satellite receiver, WLAN router, etc. The download area provides precompiled binaries for Windows, macOS, Linux, FreeBSD and Android. For other platforms the program can be compiled from source.

This is a fork of the original NZBGet project, formerly maintained by [hugbug](https://github.com/hugbug). The nzbget.com project continues where the original left off, in honor of and with respect for its maintainer of many years.

- Website: https://nzbget.com
- Downloads: https://nzbget.com/download/

## Installation

| Platform | Installation | Supported architectures / OS versions |
|---|---|---|
| **Windows** | [Releases](https://github.com/nzbgetcom/nzbget/releases) · [Winget](platforms/windows/pkgs-info.md#winget) · [Chocolatey](platforms/windows/pkgs-info.md#chocolatey) | Windows 7, 8, 10 and 11 (32-bit or 64-bit) |
| **macOS** | [Releases](https://github.com/nzbgetcom/nzbget/releases) · [Homebrew](platforms/macos/brew-info.md) | macOS Mojave 10.14+ (Intel) / macOS Big Sur 11+ (Apple Silicon) |
| **Linux** | [Releases](https://github.com/nzbgetcom/nzbget/releases) · [DEB/RPM](https://github.com/nzbgetcom/nzbget/releases) · [Repositories](https://nzbgetcom.github.io/) · [Flatpak](platforms/linux/flatpak/README.md) · [Snap](https://snapcraft.io/nzbget) · [Instructions](docs/LINUX.md) | Linux kernel 3.2+, x86 (32/64-bit), ARM 32-bit (armel, armhf), ARM 64-bit (aarch64), PowerPC (ppc6xx), RISC-V 64-bit (riscv64) |
| **FreeBSD** | [Releases](https://github.com/nzbgetcom/nzbget/releases) · [Instructions](docs/FREEBSD.md) | FreeBSD 13.0+ (x86_64 / aarch64) |
| **Docker** | [Official images](docker/README.md) · [LinuxServer.io](https://github.com/linuxserver/docker-nzbget) | x86-64 / arm64 / armv7 |
| **Synology NAS** | [SynoCommunity package](docs/SYNOLOGY.md) | DSM 7.x / 6.x / 3.x |
| **ASUSTOR NAS** | [nzbget-asustor](https://github.com/nzbgetcom/nzbget-asustor) · [App Central](https://www.asustor.com/app_central/app_detail?id=1671&type=) | ADM 4.3+ |
| **QNAP NAS** | [Package manager](platforms/linux/qnap/README.md#install-via-sherpa-package-manager) · [Manual installation](platforms/linux/qnap/README.md#manual-install) | QTS 4.1.0+ (x86_64 / x86 / arm_64 / arm-x19 / arm-x31 / arm-x41) |
| **TrueNAS SCALE** | [TrueNAS App catalog](https://apps.truenas.com/catalog/nzbget/) | x86-64 (amd64) |
| **Android** | [Releases](https://github.com/nzbgetcom/nzbget/releases) · [Android app](https://github.com/nzbgetcom/android/releases) · [Instructions](docs/ANDROID.md) | Android 5.0+ (arm64 / armv7 / x86_64 / x86) |

## Documentation

- [How to use](docs/HOW_TO_USE.md)
- [Performance tips](docs/PERFORMANCE.md)
- [RSS feeds](docs/usage/RSS.md)
- [API reference](docs/api/API.md)
- [Extensions](docs/extensions/EXTENSIONS.md)
- [Building from sources](INSTALLATION.md)

## Support

- Questions and help: [GitHub Discussions](https://github.com/nzbgetcom/nzbget/discussions)
- Bugs and feature requests: [Issues](https://github.com/nzbgetcom/nzbget/issues/new/choose)
- Security issues: see [SECURITY.md](SECURITY.md)
- Chat: [Discord](https://discord.gg/mV9Vn9sM7C)
- Contact: [nzbget.com/contact](https://nzbget.com/contact/)

## Migration from older NZBGet versions

- [Migrating from NZBGet v21 or older](https://github.com/nzbgetcom/nzbget/discussions/100#discussioncomment-8080102)
- [Migrating from older Docker images](https://github.com/nzbgetcom/nzbget/issues/84#issuecomment-1884846500)

## Contribution

Contributions are welcome — from developers and users alike. See [Contributing](docs/CONTRIBUTING.md) for the development setup, coding rules and pull request workflow.

AI-assisted contributions are allowed, but only if they follow our [AI Policy](docs/AI_POLICY.md).

## Donate

If you find NZBGet useful, please [donate](https://nzbget.com/donate/). Thank you!

## Code Signing Policy

Windows release: free code signing is provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).
