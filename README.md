# NZBGet - Efficient Usenet downloader #

[![License](https://img.shields.io/badge/license-GPL-blue.svg)](http://www.gnu.org/licenses/)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/total)
![docker pulls](https://img.shields.io/docker/pulls/nzbgetcom/nzbget.svg)

[![tests](https://github.com/nzbgetcom/nzbget/actions/workflows/tests.yml/badge.svg?branch=develop)](https://github.com/nzbgetcom/nzbget/actions/workflows/tests.yml)
[![build](https://github.com/nzbgetcom/nzbget/actions/workflows/build.yml/badge.svg?branch=develop)](https://github.com/nzbgetcom/nzbget/actions/workflows/build.yml)
[![docker build](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml/badge.svg?branch=develop)](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml)


![Contributions welcome](https://img.shields.io/badge/contributions-welcome-blue.svg)
[![GitHub issues](https://img.shields.io/github/issues/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/pulls)
![GitHub repo size](https://img.shields.io/github/repo-size/nzbgetcom/nzbget)


[![Discord](https://img.shields.io/badge/Discord-%235865F2.svg?&logo=discord&logoColor=white)](https://discord.gg/mV9Vn9sM7C)


NZBGet is a binary downloader, which downloads files from Usenet based-on information given in nzb files.

NZBGet is written in C++ and is known for its performance and efficiency.

NZBGet can run on almost any device - classic PC, NAS, media player, SAT-receiver, WLAN-router, etc. The download area provides precompiled binaries for Windows, macOS, and Linux. For other platforms, the program can be compiled from source.

This is a fork of the original NZBGet project, formerly maintained by [hugbug](https://github.com/hugbug). The nzbget.com project is an extension of the original, and is maintained in honor-of and with respect-to its maintainer of many years.  We hope to continue where the [hugbug](https://github.com/hugbug) left-off by providing a useful downloader for the benefit of the Usenet community.

More information available at https://nzbget.com

## Installation
| Platform | Installation                                                                                                                                                                                             | Supported Architectures / OS Versions                                                                                                                                 |
|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Windows** | [Releases](https://github.com/nzbgetcom/nzbget/releases) <br>[Winget](https://github.com/nzbgetcom/nzbget/blob/develop/windows/pkgs-info.md#winget) <br>[Chocolatey](https://github.com/nzbgetcom/nzbget/blob/develop/windows/pkgs-info.md#chocolatey) | Windows 7 and later, 32 or 64 bit |
| **macOS** | [Releases](https://github.com/nzbgetcom/nzbget/releases) <br>[Homebrew](https://github.com/nzbgetcom/nzbget/blob/develop/osx/brew-info.md)  | macOS Mojave 10.14+ and later (Intel / Apple Silicon) | 
| **Linux** | [Releases](https://github.com/nzbgetcom/nzbget/releases) <br>[DEB/RPM](https://github.com/nzbgetcom/nzbget/releases) [Repositories](https://nzbgetcom.github.io/) <br>[Flatpack](https://github.com/nzbgetcom/nzbget/releases) [Flatpack readme](https://github.com/nzbgetcom/nzbget/blob/develop/linux/flatpak/README.md) <br>[snap](https://snapcraft.io/nzbget) | Linux kernel 2.6 and later, x86 (32 or 64 Bit), ARM 32-bit (armel armhf), ARM 64-bit (aarch64), MIPS (mipseb mipsel), PowerPC (ppc6xx ppc500), RISC-V 64-bit (riscv64) |
| **FreeBSD** | [Releases](https://github.com/nzbgetcom/nzbget/releases) | FreeBSD 13.0+ x86_64 |
| **Docker** | [Official images](docker/README.md) <br>[LinuxServer.io](https://github.com/linuxserver/docker-nzbget) | x86-64 / arm64 / armv7  |
| <nobr>**Synology NAS** | [SynoCommunity package](docs/SYNOLOGY.md) | Synology DSM 7.x 6.x 3.x |
| <nobr>**ASUSTOR NAS** | [nzbget-asustor](https://github.com/nzbgetcom/nzbget-asustor) <br>[App Central](https://www.asustor.com/app_central/app_detail?id=1671&type=) [ASUSTOR readme](https://github.com/nzbgetcom/nzbget-asustor/blob/main/README.md) | ADM 4.3+ |
| <nobr>**QNAP NAS** | [package manager](https://github.com/nzbgetcom/nzbget/blob/develop/qnap/README.md#install-via-sherpa-package-manager) <br>[manual installation](https://github.com/nzbgetcom/nzbget/blob/develop/qnap/README.md#manual-install)  | QTS 4.1.0+ (x86_64 / x86 / arm_64 / arm-x19 / arm-x31 / arm-x41) |
| <nobr>**TrueNAS SCALE** | [TrueNAS App catalog](https://apps.truenas.com/catalog/nzbget/) | x86-64 (amd64) |
| **Android** | [Releases](https://github.com/nzbgetcom/nzbget/releases) | Android 5.0+ aarch64 |

## Migration from older NZBGet versions

[Migrating from NZBGet v21 or older](https://github.com/nzbgetcom/nzbget/discussions/100#discussioncomment-8080102)

[Migrating from older Docker images](https://github.com/nzbgetcom/nzbget/issues/84#issuecomment-1884846500)

## Building from sources

[General instructions](INSTALLATION.md)

## Extensions
 - [V1 (NZBGet v22 and below)](docs/extensions/EXTENSIONS_LEGACY.md)
 - [V2 (NZBGet v23 and above)](docs/extensions/EXTENSIONS.md)

## Brief introduction on how to use NZBGet
 - [How to use](docs/HOW_TO_USE.md)
 - [Performance tips](docs/PERFORMANCE.md)
 - [API reference](docs/api/API.md)

## Contribution

Contributions are very welcome - not only from developers, but from our users too - please don't hesitate to participate in [discussions](https://github.com/nzbgetcom/nzbget/discussions) or [create a new discussion](https://github.com/nzbgetcom/nzbget/discussions/new/choose) or [join our Discord server](https://discord.gg/mV9Vn9sM7C).

For more information - see [Contributing](docs/CONTRIBUTING.md).

## Donate

Please [donate](https://nzbget.com/donate/) if you like what we are doing. Thank you!

