#!/bin/sh

# strict error handling for debugging
set -o nounset
set -o errexit

# config variables
# urls
URL_UNRAR=https://www.rarlab.com/rar/rarmacos-x64-624.tar.gz
URL_7Z=https://www.7-zip.org/a/7z2301-mac.tar.xz
# use vcpkg libs for build and link daemon - install it via ./vcpkg install boost-json boost-optional libxml2 zlib openssl
LIB_PATH=$HOME/vcpkg/installed/x64-osx/lib
INCLUDE_PATH=$HOME/vcpkg/installed/x64-osx/include
# make jobs
JOBS=$(sysctl -n hw.ncpu)

# create directories and cleanup
mkdir -p build
rm -rf build/*
NZBGET_PATH=build/nzbget
BUILD_PATH=build/release64
mkdir $NZBGET_PATH
mkdir $BUILD_PATH

export LIBS="-liconv -lncurses $LIB_PATH/libboost_json.a $LIB_PATH/libxml2.a $LIB_PATH/libz.a $LIB_PATH/libssl.a $LIB_PATH/libcrypto.a $LIB_PATH/liblzma.a"
export INCLUDES="$INCLUDE_PATH/;$INCLUDE_PATH/libxml2/"
VERSION=$(grep "set(VERSION " CMakeLists.txt | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ $# -gt 0 ]; then
    if [ "$1" == "testing" ]; then
        VERSION_SUFFIX="-testing-$(date '+%Y%m%d')"
    fi
fi

# copy macOS project to package
cp -r osx "$NZBGET_PATH/"
DAEMON_PATH=osx/Resources/daemon/usr/local

# make static daemon binary
cd $BUILD_PATH
cmake ../.. -DENABLE_STATIC=ON -DCMAKE_INSTALL_PREFIX="$PWD/../../$NZBGET_PATH/$DAEMON_PATH" -DVERSION_SUFFIX="$VERSION_SUFFIX"
cmake --build . -j $JOBS
strip nzbget
cmake --install .
cd ../..

# fetch tools and root certificates
cd $NZBGET_PATH
mkdir -p $DAEMON_PATH/bin
rm -rf $DAEMON_PATH/etc

# 7zip
curl -o 7z.tar.xz $URL_7Z
mkdir -p 7z
tar xf 7z.tar.xz -C 7z
cp 7z/7zz $DAEMON_PATH/bin/7za

# unrar
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
xcodebuild -project osx/NZBGet.xcodeproj -configuration "Release" -destination "platform=macOS" build

# create build archive
ARCHIVE_NAME=nzbget-$VERSION$VERSION_SUFFIX-bin-macos-x64.zip
(cd osx/build/Release/ && zip -r $ARCHIVE_NAME NZBGet.app)
mv osx/build/Release/$ARCHIVE_NAME ..
