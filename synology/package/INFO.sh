#!/bin/bash

source /pkgscripts/include/pkg_util.sh

package="nzbget"
version="yyyymmdd-version"
displayname="NZBGet"
os_min_ver="7.0-41201"
maintainer="nzbget@nzbget.com"
adminport="6789"
description="NZBGet is a binary downloader, which downloads files from Usenet based on information given in nzb-files."

# populate arch
BUILD_ARCH="$(pkg_get_platform)"
arch=$BUILD_ARCH
if [ "$BUILD_ARCH" == "alpine" ];     then arch="alpine alpine4k"; fi
if [ "$BUILD_ARCH" == "armada37xx" ]; then arch="armada37xx rtd1296 rtd1619 aarch64"; fi
if [ "$BUILD_ARCH" == "avoton" ];     then arch="apollolake avoton braswell broadwell broadwellnk broadwellntb broadwellntbap bromolow cedarview coffeelake denverton geminilake grantley kvmx64 purley skylaked v1000 x86_64"; fi
if [ "$BUILD_ARCH" == "evansport" ];  then arch="evansport x86 i686"; fi

[ "$(caller)" != "0 NULL" ] && return 0
pkg_dump_info
