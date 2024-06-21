#!/bin/sh
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

# strict error handling for debugging
set -o nounset
set -o errexit

# unpackers versions
UNRAR_VERSION=701
ZIP7_VERSION=2405

# make jobs
JOBS=$(sysctl -n hw.ncpu)

# command-line params handling
ALL_ARCHS="x64 arm64"
ARCH_PARAM=""
if [ $# -gt 0 ]; then
    ARCH_PARAM=$1
fi

case $ARCH_PARAM in
    x64|arm64)
        ARCHS=$ARCH_PARAM
        ;;
    universal)
        ARCHS=$ALL_ARCHS
        ;;
    "")
        ARCH_PARAM="universal"
        ARCHS=$ALL_ARCHS
        ;;
    *)
        echo "Invalid architecture specified: $ARCH_PARAM"
        echo "Script usage: bash osx/build-nzbget.sh [arch] [testing]"
        echo "arch can be: x64 arm64 universal"
        exit 1
        ;;
esac

# version handling
VERSION=$(grep "set(VERSION " CMakeLists.txt | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ $# -gt 1 ]; then
    if [ "$2" == "testing" ]; then
        VERSION_SUFFIX="-testing-$(date '+%Y%m%d')"
    fi
fi

# create directories and cleanup
mkdir -p build
rm -rf build/*

for ARCH in $ARCHS; do

    echo "Make $ARCH nzbget package..."

    # use vcpkg libs for build and link daemon
    LIB_PATH=$HOME/vcpkg/installed/$ARCH-osx/lib
    INCLUDE_PATH=$HOME/vcpkg/installed/$ARCH-osx/include


    NZBGET_PATH=build/nzbget-$ARCH
    BUILD_PATH=build/release-$ARCH
    mkdir $NZBGET_PATH
    mkdir $BUILD_PATH

    export LIBS="-liconv -lncurses $LIB_PATH/libboost_json.a $LIB_PATH/libxml2.a $LIB_PATH/libz.a $LIB_PATH/libssl.a $LIB_PATH/libcrypto.a $LIB_PATH/liblzma.a"
    export INCLUDES="$INCLUDE_PATH/;$INCLUDE_PATH/libxml2/"

    # copy macOS project to package
    cp -r osx "$NZBGET_PATH/"
    DAEMON_PATH=osx/Resources/daemon/usr/local

    # make static daemon binary
    cd $BUILD_PATH
    if [ "$ARCH" == "x64" ]; then
        CMAKE_ARCH="x86_64"
    else
        CMAKE_ARCH=$ARCH
    fi
    
    cmake ../.. \
        -DENABLE_STATIC=ON \
        -DCMAKE_INSTALL_PREFIX="$PWD/../../$NZBGET_PATH/$DAEMON_PATH" \
        -DVERSION_SUFFIX="$VERSION_SUFFIX" \
        -DCMAKE_SYSTEM_PROCESSOR=$CMAKE_ARCH \
        -DCMAKE_OSX_ARCHITECTURES=$CMAKE_ARCH
    
    BUILD_STATUS=""
    cmake --build . -j $JOBS 2>build.log || BUILD_STATUS=$?
    if [ ! -z $BUILD_STATUS ]; then
        tail -20 build.log
        exit 1
    fi
    
    strip nzbget
    cmake --install . >/dev/null
    cd ../..

    # fetch tools and root certificates
    cd $NZBGET_PATH
    mkdir -p $DAEMON_PATH/bin
    rm -rf $DAEMON_PATH/etc

    # 7zip
    URL_7Z=https://www.7-zip.org/a/7z$ZIP7_VERSION-mac.tar.xz
    curl -o 7z.tar.xz $URL_7Z
    mkdir -p 7z
    tar xf 7z.tar.xz -C 7z
    cp 7z/7zz $DAEMON_PATH/bin/7za

    # unrar
    if [ "$ARCH" == "arm64" ]; then
        UNRAR_ARCH="arm"
    else
        UNRAR_ARCH=$ARCH
    fi
    URL_UNRAR=https://www.rarlab.com/rar/rarmacos-$UNRAR_ARCH-$UNRAR_VERSION.tar.gz
    curl -o unrar.tar.gz $URL_UNRAR
    tar -xf unrar.tar.gz
    cp rar/unrar $DAEMON_PATH/bin/unrar

    # root certificates
    curl -o $DAEMON_PATH/bin/cacert.pem https://curl.se/ca/cacert.pem

    # adjust nzbget.conf
    CONF_FILE=$DAEMON_PATH/share/nzbget/nzbget.conf
    sed -i '' 's:^MainDir=.*:MainDir=~/Library/Application Support/NZBGet:' $CONF_FILE
    sed -i '' 's:^DestDir=.*:DestDir=~/Downloads:' $CONF_FILE
    sed -i '' 's:^InterDir=.*:InterDir=~/Downloads/Intermediate:' $CONF_FILE
    sed -i '' 's:^WebDir=.*:# NOTE\: option WebDir cannot be changed because it is hardcoded in OSX version.:' $CONF_FILE
    sed -i '' 's:^LockFile=.*:# NOTE\: option LockFile cannot be changed because it is hardcoded in OSX version.:' $CONF_FILE
    sed -i '' 's:^LogFile=.*:LogFile=~/Library/Logs/NZBGet.log:' $CONF_FILE
    sed -i '' '/# example configuration file (installed to/{N;s/.*/# example configuration file (installed to\n# \/usr\/local\/share\/nzbget\/nz    bget.conf)./;}' $CONF_FILE
    sed -i '' 's:^ConfigTemplate=.*:# NOTE\: option ConfigTemplate cannot be changed because it is hardcoded in OSX version.:' $CONF_FILE
    sed -i '' 's:^DaemonUsername=.*:# NOTE\: option DaemonUsername cannot be changed because it is hardcoded in OSX version.:' $CONF_FILE
    sed -i '' 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' $CONF_FILE
    sed -i '' 's:^CertCheck=.*:CertCheck=yes:' $CONF_FILE
    sed -i '' 's:^AuthorizedIP=.*:AuthorizedIP=127.0.0.1:' $CONF_FILE
    sed -i '' 's:^ArticleCache=.*:ArticleCache=700:' $CONF_FILE
    sed -i '' 's:^DirectWrite=.*:DirectWrite=no:' $CONF_FILE
    sed -i '' 's:^WriteBuffer=.*:WriteBuffer=1024:' $CONF_FILE
    sed -i '' 's:^ParBuffer=.*:ParBuffer=500:' $CONF_FILE
    sed -i '' 's:^DirectRename=.*:DirectRename=yes:' $CONF_FILE
    sed -i '' 's:^DirectUnpack=.*:DirectUnpack=yes:' $CONF_FILE
    sed -i '' 's:^UnrarCmd=.*:UnrarCmd=${AppDir}/unrar:' $CONF_FILE
    sed -i '' 's:^SevenZipCmd=.*:SevenZipCmd=${AppDir}/7za:' $CONF_FILE

    # build macos frontend
    BUILD_STATUS=""    
    xcodebuild -project osx/NZBGet.xcodeproj -configuration "Release" -destination "platform=macOS" build >build.log 2>&1
    if [ ! -z $BUILD_STATUS ]; then
        tail -20 build.log
        exit 1
    fi

    # create build archive
    ARCHIVE_NAME=nzbget-$VERSION$VERSION_SUFFIX-bin-macos-$ARCH.zip
    (cd osx/build/Release/ && zip -r $ARCHIVE_NAME NZBGet.app >/dev/null)
    mv osx/build/Release/$ARCHIVE_NAME ..
    cd ../..
done

# make universal daemon binary and universal archive
if [ "$ARCH_PARAM" == "universal" ]; then
    echo "Make universal nzbget package..."
    cd build
    for ARCH in $ALL_ARCHS; do
        unzip nzbget-$VERSION$VERSION_SUFFIX-bin-macos-$ARCH.zip >/dev/null
        mv NZBGet.app NZBGet.$ARCH.app
    done
    DAEMON_PATH=Contents/Resources/daemon/usr/local/bin/
    # nzbget universal binary
    lipo -create NZBGet.x64.app/$DAEMON_PATH/nzbget NZBGet.arm64.app/$DAEMON_PATH/nzbget -output nzbget
    # unrar universal binary    
    lipo -create NZBGet.x64.app/$DAEMON_PATH/unrar NZBGet.arm64.app/$DAEMON_PATH/unrar -output unrar
    rm -rf NZBGet.x64.app
    mv NZBGet.arm64.app NZBGet.app
    mv nzbget NZBGet.app/$DAEMON_PATH/nzbget
    mv unrar NZBGet.app/$DAEMON_PATH/unrar
    zip -r nzbget-$VERSION$VERSION_SUFFIX-bin-macos-universal.zip NZBGet.app >/dev/null    
fi

echo "Done."
