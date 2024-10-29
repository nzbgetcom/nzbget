#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2015-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

# FreeBSD sysroot parameters
FREEBSD_VERSION="13.0"
FREEBSDIMAGE_URL="http://ftp-archive.freebsd.org/pub/FreeBSD-Archive/old-releases/amd64/$FREEBSD_VERSION-RELEASE/base.txz"

### START OF THE SCRIPT

ROOTDIR="/build/freebsd"
mkdir -p $ROOTDIR

# Download all required tools and libraries
SRCDIR=/build/source
FREEBSD_BASE="base-$FREEBSD_VERSION.txz"
mkdir -p $SRCDIR
cd $SRCDIR
if [ ! -f $FREEBSD_BASE ]; then
    curl -L -o "$SRCDIR/$FREEBSD_BASE" $FREEBSDIMAGE_URL
fi

# Creating sysroot for FreeBSD from official FreeBSD installation image.
# Our sysroot contains only libs and includes
SYSROOT="$ROOTDIR/sysroot"
rm -rf $SYSROOT
mkdir -p $SYSROOT
cd $SYSROOT
tar -xf $SRCDIR/$FREEBSD_BASE ./lib/ ./usr/lib/ ./usr/include/

cd $SYSROOT/usr/lib
# Fix broken symlinks
find . -xtype l | xargs ls -l | grep ' /lib/' | awk -v SYSROOT="$SYSROOT" '{print "ln -sf "SYSROOT$11" "$9}' | /bin/sh
ln -s libc++.a $SYSROOT/usr/lib/libstdc++.a
ln -s libc++.so $SYSROOT/usr/lib/libstdc++.so
