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
VERSION=$(grep "AC_INIT(nzbget, " configure.ac | cut -d "," -f 2 | xargs)
SPK_VERSION=$(date '+%Y%m%d')-${VERSION//./}
# if running from CI/CD, add testing to builds from non-main branch
if [ -n "$GITHUB_REF_NAME" ]; then
    if [ "$GITHUB_REF_NAME" != "main" ]; then
        NEW_VERSION="$VERSION-testing-$(date '+%Y%m%d')"
        sed -e "s|AC_INIT(nzbget.*|AC_INIT(nzbget, $NEW_VERSION, https://github.com/nzbgetcom/nzbget/issues)|g" -i configure.ac
    fi
fi
sed -e "s|version=.*$|version=\"$SPK_VERSION\"|g" -i INFO.sh

# download boost sources or take it locally
mkdir -p tmp
cd tmp
BOOST="boost-1.84.0"
if [ -f /toolkit/source/$BOOST.tar.gz ]; then
    cp /toolkit/source/$BOOST.tar.gz .
else
    wget https://github.com/boostorg/boost/releases/download/$BOOST/$BOOST.tar.gz
fi
tar zxf $BOOST.tar.gz
mv $BOOST boost
rm $BOOST.tar.gz
cd ..

# build
$TOOLKIT/pkgscripts-ng/PkgCreate.py -v 7.0 -c -P 1 nzbget $BUILD_PARAM

# remove debug packages and set user perms on packages
mv $TOOLKIT/result_spk/nzbget-$SPK_VERSION/ $TOOLKIT/result_spk/nzbget/
rm $TOOLKIT/result_spk/nzbget/*_debug.spk
chmod 666 $TOOLKIT/result_spk/nzbget/*
