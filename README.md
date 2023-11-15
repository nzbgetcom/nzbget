# NZBGet - Efficient Usenet downloader #

[![License](https://img.shields.io/badge/license-GPL-blue.svg)](http://www.gnu.org/licenses/)
![GitHub release (by tag)](https://img.shields.io/github/downloads/nzbgetcom/nzbget/v22.0/total?label=v22.0)
[![linux build](https://github.com/nzbgetcom/nzbget/actions/workflows/linux.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/linux.yml)
[![windows build](https://github.com/nzbgetcom/nzbget/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/nzbgetcom/nzbget/actions/workflows/windows.yml)
[![osx build](https://github.com/nzbgetcom/nzbget/actions/workflows/osx.yml/badge.svg)](https://github.com/nzbgetcom/nzbget/actions/workflows/osx.yml)
[![docker build](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml/badge.svg)](https://github.com/nzbgetcom/nzbget/actions/workflows/docker.yml)


![Contributions welcome](https://img.shields.io/badge/contributions-welcome-blue.svg)
[![GitHub issues](https://img.shields.io/github/issues/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/nzbgetcom/nzbget)](https://github.com/nzbgetcom/nzbget/pulls)
![GitHub repo size](https://img.shields.io/github/repo-size/nzbgetcom/nzbget)


NZBGet is a binary downloader, which downloads files from Usenet
based on information given in nzb-files. 

NZBGet is written in C++ and is known for its performance and efficiency.

NZBGet can run on almost any device - classic PC, NAS, media player, SAT-receiver, WLAN-router, etc.
The download area provides precompiled binaries for Windows, macOS, and Linux. For other platforms the program can be compiled from sources.

This is a fork of the original NZBGet project formerly maintained by [hugbug](https://github.com/hugbug). The nzbget.com project is an extension of the original, and is maintained in honor of and with respect to its maintainer of many years.  We hope to continue where the [hugbug](https://github.com/hugbug) left off by providing a useful downloader for the benefit of the Usenet community.

More information available at https://nzbget.com 

## Installation and Documentation

We provide a easy-to-use installer for each platform we support.
Please download binaries from our [releases](https://github.com/nzbgetcom/nzbget/tags) page.

We also provide a docker image for popular architectures. [Docker readme](docker/README.md)

## Building from sources

Please follow [instructions](https://nzbget.com/documentation/building-development-version/) on the website 

## Contribution

Branches naming policy

- `main` is a protected branch that contains only release code
- `develop` is a protected branch for development
- new branches should follow the following convention:
  - `hotfix/brief-description` for any small hotfixes
  - `feature/brief-description` for any new developments
  - `bugfix/brief-description` for bugs

Pull requests flow for `develop` and `main` branches:

1. For PRs targeting `develop` branch `Squash and merge` mode must be used.
2. After merging branch to `develop`, branch must be deleted.
3. For release PR (`develop` -> `main`) `Create a merge commit` mode must be used.
4. After merging `develop` -> `main`, must be back merge `main` -> `develop` before any changes in `develop` branch.

This flow results to the fact that in the PR to master branch we see only the squashed commits that correspond to the PRs in the develop branch in current release cycle.


We entice our users to participate in the project, please don't hesitate to get involved - create a [new issue](https://github.com/nzbgetcom/nzbget/issues/new) or [pull request](https://github.com/nzbgetcom/nzbget/compare)!