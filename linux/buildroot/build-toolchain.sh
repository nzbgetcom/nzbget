#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2024 phnzb <pavel@nzbget.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

set -e

ARCH=$1
if [ -z $ARCH ]; then
    echo "No architecture specified"
    exit 1
fi

# config variables
BUILDROOT_PREFIX=/build/buildroot
BUILDROOT_VERSION="2022.05.3"

# download buildroot sources and apply config
NZBGET_ROOT=$PWD
mkdir -p $BUILDROOT_PREFIX
cd $BUILDROOT_PREFIX
wget https://buildroot.org/downloads/buildroot-$BUILDROOT_VERSION.tar.gz
tar zxf buildroot-$BUILDROOT_VERSION.tar.gz
rm -rf $ARCH
mv buildroot-$BUILDROOT_VERSION $ARCH && rm buildroot-$BUILDROOT_VERSION.tar.gz
cd $BUILDROOT_PREFIX/$ARCH
cp $NZBGET_ROOT/linux/buildroot/config/.config-$ARCH .config

# revert musl to musl-1.1.24
patch package/musl/musl.mk $NZBGET_ROOT/linux/buildroot/patch/musl.mk.patch
patch package/musl/musl.hash $NZBGET_ROOT/linux/buildroot/patch/musl.hash.patch

# build toolchain
time make

# post-build patches
if [ "$ARCH" == "aarch64" ]; then
    patch $BUILDROOT_PREFIX/$ARCH/output/host/lib/gcc/aarch64-buildroot-linux-musl/9.4.0/include/arm_acle.h $NZBGET_ROOT/linux/buildroot/patch/arm_acle.h.aarch64.patch
fi
