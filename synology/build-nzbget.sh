#!/bin/bash

# synology toolkit path
TOOLKIT=/toolkit

# parameter can be build platform
BUILD_PLATFORM=$1
if [ ! -z "$BUILD_PLATFORM" ]; then BUILD_PARAM="-p $BUILD_PLATFORM"; fi

# clean up
rm -rf $TOOLKIT/source/nzbget
rm -rf $TOOLKIT/result_spk/
if [ ! -z "$BUILD_PLATFORM" ]; then
    PLATFORMS=$BUILD_PLATFORM
else
    PLATFORMS="alpine armada370 armada375 armada37xx armada38x armadaxp avoton evansport monaco"
fi
for PLATFORM in $PLATFORMS; do
    echo "Cleanup $PLATFORM environment ..."
    rm -rf $TOOLKIT/build_env/ds.$PLATFORM-7.0/image/packages
done

# copy source and prepare package structure
mkdir -p $TOOLKIT/source/nzbget 
cp -r . $TOOLKIT/source/nzbget
cp -r synology/package/* $TOOLKIT/source/nzbget/
cd $TOOLKIT/source/nzbget/
chmod +x scripts/*
chmod -x scripts/vars
chmod +x SynoBuildConf/*
chmod -x SynoBuildConf/depends
chmod +x INFO.sh

# correct build version in INFO.sh
VERSION=$(grep "AC_INIT(nzbget, " configure.ac | cut -d "," -f 2)
VERSION=${VERSION//[. ]/}
VERSION=$(date '+%Y%m%d')-$VERSION
sed -e "s|version=.*$|version=\"$VERSION\"|g" -i INFO.sh

# build
$TOOLKIT/pkgscripts-ng/PkgCreate.py -v 7.0 -c -P 2 nzbget $BUILD_PARAM

# remove debug packages and set user perms on packages
mv $TOOLKIT/result_spk/nzbget-$VERSION/ $TOOLKIT/result_spk/nzbget/
rm $TOOLKIT/result_spk/nzbget/*_debug.spk
chmod 666 $TOOLKIT/result_spk/nzbget/*
