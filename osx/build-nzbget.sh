#!/bin/sh

#
#  This file is part of nzbget. See <https://nzbget.com>.
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
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

# strict error handling for debugging
set -o nounset
set -o errexit

RESOURCES_PATH=./osx/Resources
DESTINATION_PATH=$RESOURCES_PATH/daemon/usr/local
XCODE_PROJECT=./osx/NZBGet.xcodeproj
CPUs=$(sysctl -n hw.ncpu)

mkdir -p ./tmp/x86
mkdir ./tmp/arm
mkdir ./tmp/bin
mkdir ./tmp/armrar
mkdir ./tmp/x86rar
mkdir ./tmp/7zz
mkdir ./tmp/boost

Adjust_nzbget_conf()
{
    CONF_FILE=$DESTINATION_PATH/share/nzbget/nzbget.conf
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
}

Fetch_certificate()
{
    curl -o ./tmp/bin/cacert.pem https://curl.se/ca/cacert.pem
}

Make_7za_universal()
{
    curl -o ./tmp/7zz.tar.xz https://www.7-zip.org/a/7z2301-mac.tar.xz
    tar -xf ./tmp/7zz.tar.xz -C ./tmp/7zz
    mv ./tmp/7zz/7zz ./tmp/bin/7za 
}

Make_unrar_universal()
{
    curl -o ./tmp/armrar.tar.gz https://www.rarlab.com/rar/rarmacos-arm-624.tar.gz
    curl -o ./tmp/x86rar.tar.gz https://www.rarlab.com/rar/rarmacos-x64-624.tar.gz
    tar -xf ./tmp/armrar.tar.gz -C ./tmp/armrar
    tar -xf ./tmp/x86rar.tar.gz -C ./tmp/x86rar
    lipo -create ./tmp/armrar/rar/unrar ./tmp/x86rar/rar/unrar -output ./tmp/bin/unrar
}

Make_tools()
{
    Fetch_certificate
    Make_7za_universal
    Make_unrar_universal
}

Make_OpenSSL_for_arm_and_x86()
{
    git clone --depth 1 --branch openssl-3.1.2 https://github.com/openssl/openssl.git ./tmp/openssl
    cd tmp/openssl
    mkdir ./x86
    cd ./x86
    perl ../Configure darwin64-x86_64 no-shared
    make -j $CPUs
    cd ../
    mkdir ./arm
    cd ./arm
    perl ../Configure darwin64-arm64 no-shared
    make -j $CPUs
    cd ../../../
}

Make_Boost_for_arm_and_x86()
{
    cd ./tmp/boost
    curl -LO https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz
    tar -xzvf boost-1.84.0.tar.gz
    cd boost-1.84.0
    mkdir ./x86
    mkdir ./arm
    ./bootstrap.sh --with-libraries=json,test --prefix=$(pwd)/x86
    ./b2 link=static runtime-link=static address-model=64 architecture=x86 install
    ./bootstrap.sh --with-libraries=json,test --prefix=$(pwd)/arm
    ./b2 link=static runtime-link=static address-model=64 architecture=arm install
    cd ../../../
}

Make()
{
    make -j $CPUs
    make install
    make clean
}

Compile_x86_64()
{
    ./configure --prefix=$(pwd)/tmp/x86 \
        --build=x86_64-apple-darwin \
        --target=x86_64-apple-darwin \
        --with-tlslib=OpenSSL \
        --program-prefix="" \
        CXXFLAGS="-arch x86_64 -std=c++14 -O2 -I$(pwd)/tmp/openssl/x86/include -I$(pwd)/tmp/boost/boost-1.84.0/x86/include" \
        LDFLAGS="-arch x86_64 -L$(pwd)/tmp/openssl/x86 -L$(pwd)/tmp/boost/boost-1.84.0/x86/lib"
        
    Make
}

Compile_arm()
{
    ./configure --prefix=$(pwd)/tmp/arm \
        --host=arm-apple-darwin \
        --build=arm-apple-darwin \
        --target=arm-apple-darwin \
        --with-tlslib=OpenSSL \
        --program-prefix="" \
        CXXFLAGS="-arch arm64 -std=c++14 -O2 -I$(pwd)/tmp/openssl/arm/include -I$(pwd)/tmp/boost/boost-1.84.0/arm/include" \
        LDFLAGS="-arch arm64 -L$(pwd)/tmp/openssl/arm -L$(pwd)/tmp/boost/boost-1.84.0/arm/lib"    
    Make
}

Make_universal()
{
    lipo -create ./tmp/arm/bin/nzbget ./tmp/x86/bin/nzbget -output ./tmp/bin/nzbget
}

Build()
{
    autoreconf --install
    Compile_x86_64
    Compile_arm
    Make_universal
    PLATFORM=platform=macOS
    mkdir -p $DESTINATION_PATH
    mv ./tmp/bin $DESTINATION_PATH
    mv ./tmp/x86/share $DESTINATION_PATH
    Adjust_nzbget_conf
    xcodebuild -project $XCODE_PROJECT -configuration "Release" -destination $PLATFORM build
}

Clean()
{
    make clean
    rm -rf ./tmp
    rm -rf $RESOURCES_PATH/daemon
}

Make_archive()
{
    VERSION=`grep "AC_INIT(nzbget, " configure.ac`
    VERSION=`expr "$VERSION" : '.*, \(.*\),.*)'`
    (cd osx/build/Release/ && zip -r nzbget-$VERSION-bin-macos.zip NZBGet.app)
}

Make_OpenSSL_for_arm_and_x86
Make_Boost_for_arm_and_x86
Make_tools
Build
Clean
Make_archive
