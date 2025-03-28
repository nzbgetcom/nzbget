# NZBGet - Efficient Usenet downloader #

[![License](https://img.shields.io/badge/license-GPL-blue.svg)](http://www.gnu.org/licenses/)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v22.0/total?label=v22.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v23.0/total?label=v23.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.0/total?label=v24.0)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.1/total?label=v24.1)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.2/total?label=v24.2)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.3/total?label=v24.3)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.4/total?label=v24.4)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.5/total?label=v24.5)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.6/total?label=v24.6)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.7/total?label=v24.7)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v24.8/total?label=v24.8)
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

## Installation and Documentation

We provide an easy-to-use installer for each platform we support. Please download binaries from our [releases](https://github.com/nzbgetcom/nzbget/releases) page.

Windows packages are also available via `winget` and `chocolatey` package managers. Package managers [readme](windows/pkgs-info.md)

Linux DEB/RPM packages are available from [releases](https://github.com/nzbgetcom/nzbget/releases) page or from DEB/RPM [repositories](https://nzbgetcom.github.io).

macOS packages are available from [releases](https://github.com/nzbgetcom/nzbget/releases) page or via [Homebrew](https://brew.sh) package manager. [Homebrew readme](osx/brew-info.md)

Docker images are available for x86-64 / arm64 / armv7 architectures. [Docker readme](docker/README.md). LinuxServer.io version is also available: [docker-nzbget](https://github.com/linuxserver/docker-nzbget)

Synology package is available as SynoCommunity package. [Synology readme](docs/SYNOLOGY.md)

ASUSTOR NAS package (ADM 4.3+) is available from the [nzbget-asustor](https://github.com/nzbgetcom/nzbget-asustor) repository and from the ASUSTOR [App Central](https://www.asustor.com/app_central/app_detail?id=1671&type=). [ASUSTOR readme](https://github.com/nzbgetcom/nzbget-asustor/blob/main/README.md)

QNAP packages are available as buildroot packages or via [sherpa](https://github.com/OneCDOnly/sherpa) package manager. [QNAP readme](qnap/README.md)

TrueNAS SCALE packages are available from the TrueNAS App catalog (community train).

Android packages are available for Android 5.0+. [Android readme](docs/ANDROID.md)

## Migration from older NZBGet versions

[Migrating from NZBGet v21 or older](https://github.com/nzbgetcom/nzbget/discussions/100#discussioncomment-8080102)

[Migrating from older Docker images](https://github.com/nzbgetcom/nzbget/issues/84#issuecomment-1884846500)

## Supported platforms for installers

`Windows`: Windows 7 and later, 32 or 64 Bit.

`Linux`: Linux kernel 2.6 and later, x86 (32 or 64 Bit), ARM 32-bit (armel armhf), ARM 64-bit (aarch64), MIPS (mipseb mipsel), PowerPC (ppc6xx ppc500), RISC-V 64-bit (riscv64)

`FreeBSD`: FreeBSD 13.0+ x86_64

`macOS`: X64 binary: macOS Mojave 10.14+, Universal (Intel / Apple Silicon) binary: macOS Monterey 12+

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

