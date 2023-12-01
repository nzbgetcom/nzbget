#!/bin/bash
BUILD_PLATFORM=$1
if [ ! -z "$BUILD_PLATFORM" ]; then BUILD_PARAM="-p $BUILD_PLATFORM"; fi

# clean up
rm -rf /toolkit/source/nzbget
rm -rf /toolkit/result_spk/
if [ ! -z "$BUILD_PLATFORM" ]; then
    PLATFORMS=$BUILD_PLATFORM
else
    PLATFORMS="alpine armada370 armada375 armada37xx armada38x armadaxp avoton evansport monaco"
fi
for PLATFORM in $PLATFORMS; do
    echo "Cleanup $PLATFORM environment ..."
    rm -rf /toolkit/build_env/ds.$PLATFORM-7.0/image/packages
done

# copy source and prepare package structure
mkdir -p /toolkit/source/nzbget 
cp -r . /toolkit/source/nzbget
cp -r synology/package/* /toolkit/source/nzbget/
cd /toolkit/source/nzbget/
autoreconf --install
chmod +x scripts/*
chmod +x SynoBuildConf/*
chmod +x INFO.sh

# correct build version in INFO.sh
VERSION=$(grep "AC_INIT(nzbget, " configure.ac | cut -d "," -f 2)
VERSION=${VERSION//[. ]/}
VERSION=$(date '+%Y%m%d')-$VERSION
sed -e "s|version=.*$|version=\"$VERSION\"|g" -i INFO.sh

# build
cd /toolkit/pkgscripts-ng
./PkgCreate.py -v 7.0 -c nzbget $BUILD_PARAM

# remove debug packages and set user perms on packages
mv /toolkit/result_spk/nzbget-$VERSION/ /toolkit/result_spk/nzbget/
rm /toolkit/result_spk/nzbget/*_debug.spk
chmod 666 /toolkit/result_spk/nzbget/*
