#!/bin/bash

TOOLCHAIN_PATH=/home/ph/nzbgetcom/qnap/toolchains/x86_64
CC="$TOOLCHAIN_PATH/cross-tools/bin/x86_64-QNAP-linux-gnu-gcc"
CPP="$TOOLCHAIN_PATH/cross-tools/bin/x86_64-QNAP-linux-gnu-cpp"
CXX="$TOOLCHAIN_PATH/cross-tools/bin/x86_64-QNAP-linux-gnu-g++"
STRIP="$TOOLCHAIN_PATH/cross-tools/bin/x86_64-QNAP-linux-gnu-strip"
INCLUDE="$TOOLCHAIN_PATH/fs/include"
LIB="$TOOLCHAIN_PATH/fs/lib"

export CXX=$CXX
export CPP=$CPP
export CC=$CC
export STRIP=$STRIP
export CXXFLAGS="$CXXFLAGS -I$INCLUDE"
export LDFLAGS="-L$LIB"

# build dependent libs
mkdir -p tmp
cd tmp

# build ncurses
if [ ! -d "ncurses" ]; then
    NCURSES="ncurses-5.9"
    cp /toolkit/source/$NCURSES.tar.gz .
    tar zxf $NCURSES.tar.gz
    rm $NCURSES.tar.gz
    cd $NCURSES
    ./configure --host=x86_64-QNAP-linux-gnu --prefix=$PWD/../ncurses
    make
    make install
    cd ..
    rm -rf $NCURSES
fi
export CXXFLAGS="$CXXFLAGS -I$PWD/ncurses/include"
export CPPFLAGS="$CXXFLAGS"
export LDFLAGS="$LDFLAGS -L$PWD/ncurses/lib"

# # build boost library
# BOOST="boost-1.84.0"
# if [ -f /toolkit/source/$BOOST.tar.gz ]; then
#     cp /toolkit/source/$BOOST.tar.gz .
# else
#     wget https://github.com/boostorg/boost/releases/download/$BOOST/$BOOST.tar.gz
# fi
# tar zxf $BOOST.tar.gz
# mv $BOOST boost
# rm $BOOST.tar.gz
# cd ..

# cd ./tmp/boost
# ./bootstrap.sh --with-libraries=json --prefix=$(pwd)/build
# echo "using gcc : qnap : $CXX ; " >>  project-config.jam
# ./b2 --toolset=gcc-qnap cxxstd=14 link=static runtime-link=static install
# cd ../..

# build zlib
if [ ! -d "zlib" ]; then
    ZLIB="zlib-1.3.1"
    cp /toolkit/source/$ZLIB.tar.gz .
    tar zxf $ZLIB.tar.gz
    rm $ZLIB.tar.gz

    cd $ZLIB
    ./configure --static --prefix=../zlib
    make
    make install
    cd ..
    rm -rf $ZLIB
fi
export CXXFLAGS="$CXXFLAGS -I$PWD/zlib/include"
export CPPFLAGS="$CXXFLAGS"
export LDFLAGS="$LDFLAGS -L$PWD/zlib/lib"

# build libxml2
if [ ! -d "libxml2" ]; then
    LIBXML2="libxml2-v2.12.4"
    cp /toolkit/source/$LIBXML2.tar.gz .
    tar zxf $LIBXML2.tar.gz
    rm $LIBXML2.tar.gz
    cd $LIBXML2
    ./autogen.sh --host=x86_64-QNAP-linux-gnu --enable-static --disable-shared --without-python --prefix=$PWD/../libxml2
    make
    make install
    cd ..
    rm -rf $LIBXML2
fi
export CXXFLAGS="$CXXFLAGS -I$PWD/libxml2/include/libxml2"
export CPPFLAGS="$CXXFLAGS"
export LDFLAGS="$LDFLAGS -L$PWD/libxml2/lib"

# build openssl
if [ ! -d "openssl" ]; then
    OPENSSL="openssl-3.1.2"
    cp /toolkit/source/$OPENSSL.tar.gz .
    tar zxf $OPENSSL.tar.gz
    rm $OPENSSL.tar.gz
    cd $OPENSSL
    perl Configure linux-x86_64 no-shared
    make
    make install DESTDIR=$PWD/../openssl
    cd ..
    rm -rf $OPENSSL
fi
export CXXFLAGS="$CXXFLAGS -I$PWD/openssl/usr/local/include"
export CPPFLAGS="$CXXFLAGS"
export LDFLAGS="$LDFLAGS -L$PWD/openssl/usr/local/lib64"

cd ..
autoreconf --install
export LDFLAGS="$LDFLAGS -static -s"
export CXXFLAGS="$CXXFLAGS -std=c++14 -O2"
export LIBS="-lncurses -lxml2 -lz -lm -lcrypto -ldl -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
echo $CXXFLAGS
./configure --disable-cpp-check --disable-dependency-tracking --host=x86_64-QNAP-linux-gnu

echo "export CC=$CC"
echo "export CXX=$CXX"
echo "export CPP=$CPP"
echo "export LDFLAGS=\"$LDFLAGS\""
