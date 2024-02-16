#!/bin/bash
set -e

# config variables
QNAP_ROOT=/qnap

# cleanup shared and build directories
rm -rf $QNAP_ROOT/nzbget
cp -r qnap/package $QNAP_ROOT/nzbget
NZBGET_ROOT=$PWD

# installer - first param
# if empty - find installer file in artifacts dir
INSTALLER="$1"
if [ -z $INSTALLER ]; then
    INSTALLER=$(find $NZBGET_ROOT/nzbget-linux-installers/ -name *linux.run)
fi
if [ -z $INSTALLER ]; then
    echo "Cannot find linux installer file. Exitig."
    exit 1
fi

# extract version
VERSION=$(bash $INSTALLER --help | grep 'Installer for' | cut -d ' ' -f 3 | sed -r 's/nzbget-//' | sed -r 's/-testing-.*//')

# correct version in qpkg.cfg
sed "s|^QPKG_VER=.*|QPKG_VER=\"$VERSION\"|" -i $QNAP_ROOT/nzbget/qpkg.cfg

# map of nzbget arch - qnap arch
# arm_64  - aarch64
# arm-x19 - armel
# arm-x31 - armhf
# arm-x41 - armhf
# x86_64  - x86_64
# x86     - i686
for QPKG_ARCH in arm_64 arm-x19 arm-x31 arm-x41 x86_64 x86; do
    case $QPKG_ARCH in
        arm_64)
            ARCH=aarch64;
            ;;
        arm-x19)
            ARCH=armel;
            ;;
        arm-x31)
            ARCH=armhf;
            ;;
        arm-x41)
            ARCH=armhf;
            ;;
        x86)
            ARCH=i686;
            ;;
        *)
            ARCH=$QPKG_ARCH;
            ;;
    esac
    bash $INSTALLER --destdir $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget --arch $ARCH --silent
    cd $QNAP_ROOT/nzbget
    qbuild --build-arch $QPKG_ARCH
    cd $NZBGET_ROOT
done
