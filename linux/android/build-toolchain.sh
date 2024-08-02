#!/bin/bash
#
#  This file is part of nzbget. See <http://nzbget.net>.
#
#  Copyright (C) 2018 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

# This script builds cross-compiling toolchain, which can compile NZBGet for Android.
# The toolchain itself runs on Linux.

# Setup strict bash error handling
set -e

# Android API level
APILEVEL=21

# Architecture
ARCH=$1
case "$ARCH" in
    i686)
        NDK_ARCH="x86"
        NDK_TARGET="i686-linux-android"
        ;;
    x86_64)
        NDK_ARCH="x86_64"
        NDK_TARGET="x86_64-linux-android"
        ;;
    armhf)
        NDK_ARCH="arm"
        NDK_TARGET="arm-linux-androideabi"
        ;;
    aarch64)
        NDK_ARCH="arm64"
        NDK_TARGET="aarch64-linux-android"
        ;;
    *)
        echo "Usage: $0 (i686|x86_64|armhf|aarch64)"
        exit 1
        ;;
esac

echo "Creating toolchain for $ARCH"

# Android NDK
NDK_VERSION="r19c"
NDK_DIRNAME="android-ndk-$NDK_VERSION"
NDK_ARCHIVE="$NDK_DIRNAME-linux-x86_64.zip"
NDK_URL="https://dl.google.com/android/repository/$NDK_ARCHIVE"

### START OF THE SCRIPT

ROOTDIR="/build/android"
ROOTDIR="$ROOTDIR/$ARCH-ndk"

rm -rf $ROOTDIR
mkdir -p $ROOTDIR

# Download all required tools and libraries
cd ..
SRCDIR=/build/source
mkdir -p $SRCDIR
cd $SRCDIR
if [ ! -d android-ndk -a ! -f $NDK_ARCHIVE ]; then
    wget $NDK_URL
fi
cd $ROOTDIR
cd ..

# Unpack NDK
if [ ! -d android-ndk ]; then
    echo "Unpacking NDK"
    unzip $SRCDIR/$NDK_ARCHIVE
    mv $NDK_DIRNAME android-ndk
fi

# Create toolchain for target
echo "Preparing standalone NDK toolchain"
python3 android-ndk/build/tools/make_standalone_toolchain.py --arch $NDK_ARCH --api $APILEVEL --install-dir $ROOTDIR/output/host/usr
cd $ROOTDIR
ln -s host/usr/sysroot output/staging
echo "Toolchain creation completed for $ARCH"
