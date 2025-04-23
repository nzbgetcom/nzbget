#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2025 phnzb <pavel@nzbget.com>
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

# stop on error
set -e

NZBGET_ROOT=$PWD

# $1 - installer
# if empty - find installer file in artifacts dir
INSTALLER="$1"
if [ -z $INSTALLER ]; then
    INSTALLER=$(find $NZBGET_ROOT/nzbget-linux-installers/ -name *linux.run 2>/dev/null) || true
fi
if [ -z $INSTALLER ]; then
    echo "Cannot find linux installer file. Exitig."
    exit 1
fi

# extract version
VERSION=$(bash "$INSTALLER" --help | grep 'Installer for' | cut -d ' ' -f 3 | sed -r 's/nzbget-//')

# prepare build files
rm -rf build/flatpak
mkdir -p build/flatpak
cp -r linux/flatpak/* build/flatpak
cp $INSTALLER build/flatpak/nzbget-bin-linux.run

# correct version in metainfo file
RELEASE_INFO="<release version=\"$VERSION\" date=\"$(date +%F)\">"
sed -i "s|^    <release version=.*|    $RELEASE_INFO|g" build/flatpak/com.nzbget.nzbget.metainfo.xml

# build
cd build/flatpak
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --default-branch=$VERSION --force-clean --sandbox --user --install-deps-from=flathub --ccache --repo=repo build com.nzbget.nzbget.yml
flatpak build-bundle repo "com.nzbget.nzbget.$VERSION.x86_64.flatpak" com.nzbget.nzbget $VERSION
