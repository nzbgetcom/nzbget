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
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# strict error handling for debugging
set -o nounset
set -o errexit

# install Homebrew package manager (https://brew.sh/)
/bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# install build tools
xcode-select --install

# install dependencies
brew install git
brew install zlib
brew install libxml2
brew install ncurses
brew install openssl@3 # TODO: try to build without this package
                       # Without this package, pkg-config swears that it can't find openssl, 
                       # even when I manually specified all the openssl paths
brew install pkg-config

CPUs=$(sysctl -n hw.ncpu)

mkdir -p ./tmp/x86
mkdir ./tmp/arm
mkdir ./tmp/bin

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
        --program-prefix=x86 \
        CXXFLAGS="-arch x86_64 -I$(pwd)tmp/openssl/x86/include" \
        LDFLAGS="-arch x86_64 -L$(pwd)/tmp/openssl/x86"
        
    Make
}

Compile_arm()
{
    ./configure --prefix=$(pwd)/tmp/arm \
        --host=arm-apple-darwin \
        --build=arm-apple-darwin \
        --target=arm-apple-darwin \
        --with-tlslib=OpenSSL \
        --program-prefix=arm \
        CXXFLAGS="-arch arm64 -I$(pwd)tmp/openssl/arm/include" \
        LDFLAGS="-arch arm64 -L$(pwd)/tmp/openssl/arm"

    Make
}

Make_universal()
{
    lipo -create ./tmp/arm/bin/armnzbget ./tmp/x86/bin/x86nzbget -output ./tmp/bin/nzbget
    mv ./tmp/x86/share ./tmp/share
    rm -rf ./tmp/x86 ./tmp/arm
}

Build()
{
    Compile_x86_64
    Compile_arm
    Make_universal
    DESTINATION_PATH="./osx/Resources/daemon/usr/local"
    XCODE_PROJECT="./osx/NZBGet.xcodeproj"
    PLATFORM="platform=macOS"
    mkdir -p $DESTINATION_PATH
    mv ./tmp/bin $DESTINATION_PATH
    mv ./tmp/share $DESTINATION_PATH
    xcodebuild -project $XCODE_PROJECT -configuration "Release" -destination $PLATFORM build
}

Clean()
{
    rm -rf ./tmp
    rm -rf ./osx/Resources/daemon
}

Make_OpenSSL_for_arm_and_x86
Build
Clean
