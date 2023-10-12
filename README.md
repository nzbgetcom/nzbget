# NZBGet - Efficient Usenet downloader #
[![License](https://img.shields.io/badge/license-GPL-blue.svg)](http://www.gnu.org/licenses/)
[![Code Quality: Cpp](https://img.shields.io/lgtm/grade/cpp/g/nzbget/nzbget.svg?label=code%20quality:%20c%2b%2b)](https://lgtm.com/projects/g/nzbget/nzbget/context:cpp)
[![Code Quality: JavaScript](https://img.shields.io/lgtm/grade/javascript/g/nzbget/nzbget.svg?label=code%20quality:%20js)](https://lgtm.com/projects/g/nzbget/nzbget/context:javascript)
[![Total Alerts](https://img.shields.io/lgtm/alerts/g/nzbget/nzbget.svg)](https://lgtm.com/projects/g/nzbget/nzbget/alerts)

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

## Building from sources

Please follow [instructions](https://nzbget.com/documentation/building-development-version/) on the website 

## Contribution

Branches naming policy

- `main` is a protected branch that constains only release code
- `develop` is a protected branch for development
- new branches should follow the following convention:
  - `hotfix/brief-description` for any small hotfixes
  - `feature/brief-description` for any new developments
  - `bugfix/brief-description` for bugs

Pull requests flow for `develop` and `main` branches:

1. For PRs targeting `develop` branch `Squash and merge` mode must be used.
2. For release PR (`develop` -> `main`) `Create a merge commit` mode must be used.
3. After merging `develop` -> `main`, must be back merge `main` -> `develop` before any changes in `develop` branch.

This flow results to the fact that in the PR to master branch we see only the squashed commits that correspond to the PRs in the develop branch in current release cycle.


We entice our users to participate in the project, please don't hesitate to get involved - create a [new issue](https://github.com/nzbgetcom/nzbget/issues/new) or [pull request](https://github.com/nzbgetcom/nzbget/compare)!