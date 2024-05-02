#!/bin/bash
set -e

ARCH=$1
if [ -z $ARCH ]; then
    echo "No architecture specified"
    exit 1
fi

# config variables
BUILDROOT_PREFIX=/build/buildroot
BUILDROOT_VERSION="2022.05.3"

NZBGET_ROOT=$PWD
mkdir -p $BUILDROOT_PREFIX
cd $BUILDROOT_PREFIX
wget https://buildroot.org/downloads/buildroot-$BUILDROOT_VERSION.tar.gz
tar zxf buildroot-$BUILDROOT_VERSION.tar.gz
rm -rf $ARCH
mv buildroot-$BUILDROOT_VERSION $ARCH && rm buildroot-$BUILDROOT_VERSION.tar.gz
cd $BUILDROOT_PREFIX/$ARCH
cp $NZBGET_ROOT/linux/buildroot/config/.config-$ARCH .config
patch package/musl/musl.mk $NZBGET_ROOT/linux/buildroot/patch/musl.mk.patch
patch package/musl/musl.hash $NZBGET_ROOT/linux/buildroot/patch/musl.hash.patch
time make
