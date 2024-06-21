# NZBGet - Efficient Usenet downloader #

[![License](https://img.shields.io/badge/license-GPL-blue.svg)](http://www.gnu.org/licenses/)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v22.0/total?label=v22.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v23.0/total?label=v23.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.0/total?label=v24.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.1/total?label=v24.1)
![docker pulls](https://img.shields.io/docker/pulls/nzbgetcom/nzbget.svg)

[![linux build](https://github.com/nzbgetcom/nzbget/actions/workflows/linux.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/linux.yml)
[![windows build](https://github.com/nzbgetcom/nzbget/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/windows.yml)
[![osx build](https://github.com/nzbgetcom/nzbget/actions/workflows/osx.yml/badge.svg)](https://github.com/nzbgetcom/nzbget/actions/workflows/osx.yml)
[![docker build](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml/badge.svg)](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml)
[![qnap repack](https://github.com/nzbgetcom/nzbget/actions/workflows/qnap-repack.yml/badge.svg)](https://github.com/nzbgetcom/nzbget/actions/workflows/qnap-repack.yml)


![Contributions welcome](https://img.shields.io/badge/contributions-welcome-blue.svg)
[![GitHub issues](https://img.shields.io/github/issues/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/pulls)
![GitHub repo size](https://img.shields.io/github/repo-size/nzbgetcom/nzbget)


NZBGet is a binary downloader, which downloads files from Usenet based-on information given in nzb files.

NZBGet is written in C++ and is known for its performance and efficiency.

NZBGet can run on almost any device - classic PC, NAS, media player, SAT-receiver, WLAN-router, etc. The download area provides precompiled binaries for Windows, macOS, and Linux. For other platforms, the program can be compiled from source.

This is a fork of the original NZBGet project, formerly maintained by [hugbug](https://github.com/hugbug). The nzbget.com project is an extension of the original, and is maintained in honor-of and with respect-to its maintainer of many years.  We hope to continue where the [hugbug](https://github.com/hugbug) left-off by providing a useful downloader for the benefit of the Usenet community.

More information available at https://nzbget.com

## Installation and Documentation

We provide an easy-to-use installer for each platform we support. Please download binaries from our [releases](https://github.com/nzbgetcom/nzbget/releases) page.

Windows packages are also available via `winget` and `chocolatey` package managers. Package managers [readme](windows/pkgs-info.md)

Linux DEB/RPM packages are available from [releases](https://github.com/nzbgetcom/nzbget/releases) page or from DEB/RPM [repositories](https://nzbgetcom.github.io).

macOS packages are available from [releases](https://github.com/nzbgetcom/nzbget/releases) page or via [Homebrew](https://brew.sh) package manager. [Homebrew readme](osx/brew-info.md)

Docker images are available for x86-64 / arm64 / armv7 architectures. [Docker readme](docker/README.md). LinuxServer.io version is also available: [docker-nzbget](https://github.com/linuxserver/docker-nzbget)

Synology package are available as SynoCommunity package. [Synology readme](docs/SYNOLOGY.md)

QNAP packages are available as buildroot packages or via [sherpa](https://github.com/OneCDOnly/sherpa) package manager. [QNAP readme](qnap/README.md)

## Migration from older NZBGet versions

[Migrating from NZBGet v21 or older](https://github.com/nzbgetcom/nzbget/discussions/100#discussioncomment-8080102)

[Migrating from older Docker images](https://github.com/nzbgetcom/nzbget/issues/84#issuecomment-1884846500)

## Supported platforms for installers

`Windows`: Windows 7 and later, 32 or 64 Bit.

`Linux`: Linux kernel 2.6 and later, x86 (32 or 64 Bit), ARM 32-bit (armel armhf), ARM 64-bit (aarch64), MIPS (mipseb mipsel), PowerPC (ppc6xx ppc500), RISC-V 64-bit (riscv64)

`macOS`: X64 binary: macOS Mojave 10.14+, Universal (Intel / Apple Silicon) binary: macOS Monterey 12+

## Building from sources

[General instructions](INSTALLATION.md)

## Extensions
 - [V1 (NZBGet v22 and below)](docs/extensions/EXTENSIONS_LEGACY.md)
 - [V2 (NZBGet v23 and above)](docs/extensions/EXTENSIONS.md)

## Brief introduction on how to use NZBGet
 - [How to use](docs/HOW_TO_USE.md)
 - [Performance tips](docs/PERFORMANCE.md)

## Contribution

Contributions are very welcome - not only from developers, but from our users too - please don't hesitate to participate in [discussions](https://github.com/nzbgetcom/nzbget/discussions) or [create a new discussion](https://github.com/nzbgetcom/nzbget/discussions/new/choose)

For more information - see [Contributing](docs/CONTRIBUTING.md).

## Donate

Please [donate](https://nzbget.com/donate/) if you like what we are doing. Thank you!

