#!/bin/bash
set -e

# config variables
BUILDROOT_HOME=/build
TOOLCHAIN_PATH=$BUILDROOT_HOME/buildroot
LIB_SRC_PATH=$BUILDROOT_HOME/source
LIB_PATH=$BUILDROOT_HOME/lib
ALL_ARCHS="armhf"
COREX=4

download_lib_source()
{
    LIB=$1
    URL=$2
    LIB_SRC_FILE=${URL##*/}
    if [ ! -f "$LIB_SRC_PATH/$LIB_SRC_FILE" ]; then
        echo "Downloading $LIB_SRC_FILE ..."
        mkdir -p "$LIB_SRC_PATH"
        curl -o "$LIB_SRC_PATH/$LIB_SRC_FILE" -lL $URL
    fi
}

build_lib()
{
    URL=$1
    LIB_SRC_FILE=${URL##*/}
    LIB=$(echo $LIB_SRC_FILE | cut -d- -f 1)
    download_lib_source $LIB $URL
    if [ ! -d "$LIB_PATH/$ARCH/$LIB" ]; then
        mkdir -p "$LIB_PATH/$ARCH"
        cp "$LIB_SRC_PATH/$LIB_SRC_FILE" "$LIB_PATH/$ARCH"
        cd "$LIB_PATH/$ARCH"
        tar zxf "$LIB_SRC_FILE"
        rm $LIB_SRC_FILE
        cd ${LIB_SRC_FILE/.tar.gz/}
        case $LIB in
            "ncurses")                
                ./configure --with-termlib --without-progs --host=$HOST --prefix="$PWD/../$LIB"
                ;;
            "zlib")
                ./configure --static --prefix="$PWD/../$LIB"
                ;;
            "libxml2")
                ./autogen.sh --host=$HOST --enable-static --disable-shared --without-python --prefix="$PWD/../$LIB"
                ;;
            "openssl")
                case $ARCH in
                    "i686")
                        OPENSSL_ARCH=generic32
                        ;;
                    "armhf")
                        OPENSSL_ARCH=armv4
                        ;;
                    *)
                        OPENSSL_ARCH=$ARCH
                        ;;
                esac                
                perl Configure linux-$OPENSSL_ARCH no-shared --prefix="$PWD/../$LIB"
                ;;
            "boost")
                ./bootstrap.sh --with-libraries=json --prefix="$PWD/../$LIB"
                echo "using gcc : buildroot : $CXX ; " >>  project-config.jam
                ./b2 --toolset=gcc-buildroot cxxstd=14 link=static runtime-link=static install
                ;;
        esac
        if [ "$LIB" != "boost" ]; then
            make -j $COREX
            make install
        fi
        cd ..
        rm -rf ${LIB_SRC_FILE/.tar.gz/}
    fi
    if [ "$LIB" == "libxml2" ]; then
        export CXXFLAGS="$CXXFLAGS -I$LIB_PATH/$ARCH/$LIB/include/libxml2"
    else
        export CXXFLAGS="$CXXFLAGS -I$LIB_PATH/$ARCH/$LIB/include"
    fi
    export CPPFLAGS="$CXXFLAGS"
    if [ "$LIB" == "openssl" ]; then
        export LDFLAGS="$LDFLAGS -L$LIB_PATH/$ARCH/$LIB/lib -L$LIB_PATH/$ARCH/$LIB/lib64"
    else
        if [ -z "$LDFLAGS" ]; then
            export LDFLAGS="-L$LIB_PATH/$ARCH/$LIB/lib"
        else
            export LDFLAGS="$LDFLAGS -L$LIB_PATH/$ARCH/$LIB/lib"
        fi
    fi
    if [ "$LIB" == "libxml2" ]; then
        export INCLUDES="$INCLUDES$LIB_PATH/$ARCH/$LIB/include/libxml2/;"
    elif [ "$LIB" == "ncurses" ]; then
        export INCLUDES="$INCLUDES$LIB_PATH/$ARCH/$LIB/include/;$INCLUDES$LIB_PATH/$ARCH/$LIB/include/ncurses/;"
    else
        export INCLUDES="$INCLUDES$LIB_PATH/$ARCH/$LIB/include/;"
    fi
    cd $NZBGET_ROOT
}

# cleanup shared and build directories
mkdir -p build
rm -rf build/*
NZBGET_ROOT=$PWD

for ARCH in $ALL_ARCHS; do
    
    # toolchain variables
    export ARCH=$ARCH
    export HOST="arm-buildroot-linux-musleabihf"
    export CC="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-gcc"
    export CPP="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-cpp"
    export CXX="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-g++"
    export AR="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-ar"
    export STRIP="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-strip"

    # clean build flags
    export CXXFLAGS=""
    export CPPFLAGS=""
    export LDFLAGS=""
    export LIBS=""
    export INCLUDES="$TOOLCHAIN_PATH/$ARCH/output/staging/usr/include/;"

    build_lib "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz"
    build_lib "https://zlib.net/zlib-1.3.1.tar.gz"
    build_lib "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.12.4/libxml2-v2.12.4.tar.gz"
    build_lib "https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-3.1.2.tar.gz"
    build_lib "https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz"

    # build_7zip
    # build_unrar

    export LIBS="$LDFLAGS -lxml2 -lrt -lboost_json -lz -lssl -lcrypto -lncurses -ltinfo -latomic -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
    unset CXXFLAGS
    unset CPPFLAGS
    unset LDFLAGS
   
    mkdir -p build/$ARCH
    cmake -S . -B build/$ARCH \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=arm \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake \
        -DTOOLCHAIN_PREFIX=$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST \
        -DENABLE_STATIC=ON
    cmake --build build/$ARCH -j $COREX || true
    exit 1
    
    SHARED=$QNAP_ROOT/nzbget/shared
    if [ ! -d "$SHARED/nzbget" ]; then
        # populate shared folder
        rm -rf $SHARED/install
        make install DESTDIR=$SHARED/install
        cd $SHARED
        rm -rf nzbget
        mkdir -p nzbget
        mv install/usr/local/share/doc/nzbget/* nzbget
        mv install/usr/local/share/nzbget/webui nzbget
        mv install/usr/local/share/nzbget/scripts nzbget
        CONFTEMPLATE=nzbget/webui/nzbget.conf.template
        mv install/usr/local/share/nzbget/nzbget.conf $CONFTEMPLATE

        rm -rf install

        # adjusting nzbget.conf
        sed 's|^MainDir=.*|MainDir=${AppDir}/downloads|' -i $CONFTEMPLATE
        sed 's|^DestDir=.*|DestDir=${MainDir}/completed|' -i $CONFTEMPLATE
        sed 's|^InterDir=.*|InterDir=${MainDir}/intermediate|' -i $CONFTEMPLATE
        sed 's|^WebDir=.*|WebDir=${AppDir}/webui|' -i $CONFTEMPLATE
        sed 's|^ScriptDir=.*|ScriptDir=${AppDir}/scripts|' -i $CONFTEMPLATE
        sed 's|^LogFile=.*|LogFile=${MainDir}/nzbget.log|' -i $CONFTEMPLATE
        sed 's|^ConfigTemplate=.*|ConfigTemplate=${AppDir}/webui/nzbget.conf.template|' -i $CONFTEMPLATE
        sed 's|^AuthorizedIP=.*|AuthorizedIP=127.0.0.1|' -i $CONFTEMPLATE
        sed 's|^CertCheck=.*|CertCheck=yes|' -i $CONFTEMPLATE
        sed 's|^CertStore=.*|CertStore=${AppDir}/cacert.pem|' -i $CONFTEMPLATE
        sed 's|^UnrarCmd=.*|UnrarCmd=${AppDir}/unrar|' -i $CONFTEMPLATE
        sed 's|^SevenZipCmd=.*|SevenZipCmd=${AppDir}/7za|' -i $CONFTEMPLATE

        cp $CONFTEMPLATE nzbget/nzbget.conf
        curl -o nzbget/cacert.pem -L "https://curl.se/ca/cacert.pem"
    fi
    
    cd $NZBGET_ROOT
    mkdir -p $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget
    # copy main executable
    cp nzbget $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget/
    # copy unrar / 7zip
    cp nzbget $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget/
    cp $LIB_PATH/$ARCH/7zip/* $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget/
    cp $LIB_PATH/$ARCH/unrar/* $QNAP_ROOT/nzbget/$QPKG_ARCH/nzbget/
    cd $QNAP_ROOT/nzbget
    qbuild --build-arch $QPKG_ARCH
    cd $NZBGET_ROOT
done