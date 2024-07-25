#!/bin/bash
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2024 phnzb <pavel@nzbget.com>
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

set -e

# config varibles (defaults, can be overrided from command-line)
PLATFORMS=""
ARCHS=""
OUTPUTS=""
CONFIGS=""
COREX=4
TESTING="no"

# build variables
ALL_ARCHS="armel armhf aarch64 i686 x86_64 riscv64 mipsel mipseb ppc500 ppc6xx i686-ndk x86_64-ndk armhf-ndk aarch64-ndk"
ALL_PLATFORMS="linux android"
OUTPUTDIR=build
BUILDROOT_HOME=/build
LIB_SRC_PATH=$BUILDROOT_HOME/source
LIB_PATH=$BUILDROOT_HOME/lib

# unpackers versions
UNRAR6_VERSION=6.2.12
UNRAR7_VERSION=7.0.7
ZIP7_VERSION=2405

# libs versions
NCURSES_VERSION=6.4
ZLIB_VERSION=1.3.1
LIBXML2_VERSION=2.12.4
OPENSSL_VERSION=3.1.2
BOOST_VERSION=1.84.0

help()
{
    echo "Usage:"
    echo "  $(basename $0) [platform] [architectures] [output] [configs] [testing] [corex]"
    echo "    platform      : all (default) $ALL_ARCHS"
    echo "    architectures : all (default) $ALL_PLATFORMS"
    echo "    output        : bin installer"
    echo "    testing       : build testing image"
    echo "    configs       : release (default) debug"
    echo "    corex         : multicore make (x is a number of threads) 4 is default"
    echo
}

parse_args()
{
    for PARAM in "$@"
    do
        case $PARAM in
            android|linux)
                PLATFORMS=`echo "$PLATFORMS $PARAM" | xargs`
                ;;
            release|debug)
                CONFIGS=`echo "$CONFIGS $PARAM" | xargs`
                ;;
            testing)
                TESTING=yes
                ;;
            bin|installer)
                OUTPUTS=`echo "$OUTPUTS $PARAM" | xargs`
                ;;
            core[1-9])
                COREX="${PARAM:4}"
                ;;
            help)
                help
                exit 0
                ;;
            *)
                if [[ " $ALL_ARCHS " == *" $PARAM "* ]]; then
                    ARCHS=`echo "$ARCHS $PARAM" | xargs`
                    if [ "$PARAM" == "all" ]; then
                        PARAM=$ALL_ARCHS
                    fi
                else
                    echo "Invalid parameter: $PARAM"
                    help
                    exit 1
                fi
                ;;
        esac
    done

    if [ "$PLATFORMS" == "" ]; then
        PLATFORMS="$ALL_PLATFORMS"
    fi

    if [ "$ARCHS" == "" ]; then
        ARCHS="$ALL_ARCHS"
    fi

    if [ "$OUTPUTS" == "" ]; then
        OUTPUTS="bin installer"
    fi

    if [ "$CONFIGS" == "" ]; then
        CONFIGS="release debug"
    fi

    # filter unneeded platforms
    FILTERED_PLATFORMS=""
    for PLATFORM in $PLATFORMS; do
        filter_archs
        if [ "$PLATFORM_ARCHS" != "" ]; then
            FILTERED_PLATFORMS=`echo "$FILTERED_PLATFORMS $PLATFORM" | xargs`
        fi
    done
    PLATFORMS=$FILTERED_PLATFORMS

    # filter unneeded archs
    FILTERED_ARCHS=""
    for ARCH in $ARCHS; do
        ARCH_NEED=0
        for PLATFORM in $PLATFORMS; do
            if [[ $ARCH == *-ndk ]] && [ "$PLATFORM" == "android" ]; then
                ARCH_NEED=1
            fi
            if [[ $ARCH != *-ndk ]] && [ "$PLATFORM" == "linux" ]; then
                ARCH_NEED=1
            fi
        done
        if [ $ARCH_NEED -eq 1 ]; then
            FILTERED_ARCHS=`echo "$FILTERED_ARCHS $ARCH" | xargs`
        fi
    done
    ARCHS=$FILTERED_ARCHS
}

print_config()
{
    echo "Active configuration:"
    echo "  platforms      : $PLATFORMS"
    echo "  architectures  : $ARCHS"
    echo "  outputs        : $OUTPUTS"
    echo "  configs        : $CONFIGS"
    echo "  testing        : $TESTING"
    echo "  cores          : $COREX"
    echo
}

construct_suffix()
{
    if [ "$CONFIG" == "debug" ]; then
        SUFFIX="-debug"
    else
        SUFFIX=""
    fi
}

filter_archs()
{
    PLATFORM_ARCHS=""
    for ARCH in $ARCHS
    do
        if [[ $ARCH == *-ndk ]] && [ "$PLATFORM" == "android" ]; then
            PLATFORM_ARCHS=`echo "$PLATFORM_ARCHS $ARCH" | xargs`
        fi
        if [[ $ARCH != *-ndk ]] && [ "$PLATFORM" == "linux" ]; then
            PLATFORM_ARCHS=`echo "$PLATFORM_ARCHS $ARCH" | xargs`
        fi
    done
}

get_short_arch()
{
    ARCH_SHORT="${ARCH/-ndk/}"
}

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
            ncurses)
                ./configure \
              	--without-cxx \
                --without-cxx-binding \
                --without-ada \
                --without-tests \
                --disable-big-core \
                --without-profile \
                --disable-rpath \
                --disable-rpath-hack \
                --enable-echo \
                --enable-const \
                --enable-overwrite \
                --enable-pc-files \
                --disable-stripping \
                --without-manpages \
                --with-fallbacks="xterm xterm-color xterm-256color xterm-16color linux vt100 vt200" \
                --without-shared \
                --with-normal \
                --without-gpm \
                --enable-ext-colors \
                --without-debug \
                --enable-widec \
                --host=$HOST \
                --prefix="$PWD/../$LIB"
                ;;
            zlib)
                ./configure --static --prefix="$PWD/../$LIB"
                ;;
            libxml2)
                ./autogen.sh --host=$HOST \
                --enable-static \
                --disable-shared \
                --with-gnu-ld \
                --without-python \
                --without-debug \
                --without-icu \
                --without-lzma \
                --without-iconv \
                --prefix="$PWD/../$LIB"
                ;;
            openssl)
                OPENSSL_OPTS=""
                case $ARCH in
                    i686)
                        OPENSSL_ARCH=linux-generic32
                        ;;
                    armel)
                        OPENSSL_ARCH=linux-armv4
                        ;;
                    armhf)
                        OPENSSL_ARCH=linux-armv4
                        ;;
                    riscv64)
                        OPENSSL_ARCH=linux64-riscv64
                        ;;
                    mipseb)
                        OPENSSL_ARCH=linux-mips32
                        ;;
                    mipsel)
                        OPENSSL_ARCH=linux-mips32
                        ;;
                    ppc6xx)
                        OPENSSL_ARCH=linux-ppc
                        ;;
                    ppc500)
                        OPENSSL_ARCH=linux-ppc
                        OPENSSL_OPTS=no-async
                        ;;
                    *-ndk)
                        _CC=$CC
                        unset CC
                        _CPP=$CPP
                        unset CPP
                        _CXX=$CXX
                        unset CXX
                        _AR=$AR
                        unset AR
                        _STRIP=$STRIP
                        unset STRIP
                        OPENSSL_OPTS="-D__ANDROID_API__=21 no-asm"
                        export ANDROID_NDK_ROOT="$TOOLCHAIN_PATH/android-ndk"
                        case $ARCH in
                            i686-ndk)
                                OPENSSL_ARCH=android-x86
                                ANDROID_TOOLCHAIN=x86-4.9
                                ;;
                            x86_64-ndk)
                                OPENSSL_ARCH=android-x86_64
                                ANDROID_TOOLCHAIN=x86_64-4.9
                                ;;
                            armhf-ndk)
                                OPENSSL_ARCH=android-arm
                                ANDROID_TOOLCHAIN=arm-linux-androideabi-4.9
                                ;;
                            aarch64-ndk)
                                OPENSSL_ARCH=android-arm64
                                ANDROID_TOOLCHAIN=aarch64-linux-android-4.9
                                ;;
                        esac
                        _PATH=$PATH
                        export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/$ANDROID_TOOLCHAIN/prebuilt/linux-x86_64/bin:$PATH"
                        ;;
                    *)
                        OPENSSL_ARCH=linux-$ARCH
                        ;;
                esac
                perl Configure $OPENSSL_ARCH \
                    no-shared \
                    threads \
                    no-rc5 \
                    enable-camellia \
                    no-tests \
                    no-fuzz-libfuzzer \
                    no-fuzz-afl \
                    no-afalgeng \
                    zlib \
                    no-dso \
                    $OPENSSL_OPTS \
                    --prefix="$PWD/../$LIB"
                ;;
            boost)
                ./bootstrap.sh --with-libraries=json --prefix="$PWD/../$LIB"
                echo "using gcc : buildroot : $CXX ; " >>  project-config.jam
                ./b2 --toolset=gcc-buildroot cxxstd=14 link=static runtime-link=static install
                ;;
        esac
        if [ "$LIB" != "boost" ]; then
            make -j $COREX
            if [ "$LIB" == "openssl" ]; then
                make install_sw
            else
                make install
            fi
        fi
        # restore vars after android openssl build
        if [ "$LIB" == "openssl" ]  && [ "$PLATFORM" == "android" ] ; then
            export CC=$_CC
            export CPP=$_CPP
            export CXX=$_CXX
            export AR=$_AR
            export STRIP=$_STRIP
            export PATH=$_PATH
            unset ANDROID_NDK_ROOT
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
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/libxml2/;"
    elif [ "$LIB" == "ncurses" ]; then
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/;$LIB_PATH/$ARCH/$LIB/include/ncurses/;"
    else
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/;"
    fi
    cd $NZBGET_ROOT
}

build_7zip()
{
    if [ ! -d "$LIB_PATH/$ARCH/7zip" ]; then
        rm -rf /tmp/7z
        mkdir -p /tmp/7z
        curl -o /tmp/7z/7z.tar.xz -lL https://www.7-zip.org/a/7z$ZIP7_VERSION-src.tar.xz
        cd /tmp/7z
        tar xf 7z.tar.xz
        rm 7z.tar.xz
        cd CPP/7zip
        sed "s|^LDFLAGS_STATIC =.*|LDFLAGS_STATIC = -static|" -i 7zip_gcc.mak
        if [ "$PLATFORM" == "android" ]; then
            sed "s|^#if defined(TIME_UTC)|#if defined(_TIME_UTC)|g" -i ../Windows/TimeUtils.cpp
            sed "s|^LIB2 =.*|LIB2 = |g" -i 7zip_gcc.mak
            sed "s|^CFLAGS_WARN_WALL =.*|CFLAGS_WARN_WALL = -Wall -Wextra|" -i 7zip_gcc.mak
        fi
        cd Bundles/Alone
        make -j $COREX -f makefile.gcc
        mkdir -p $LIB_PATH/$ARCH/7zip
        cp _o/7za $LIB_PATH/$ARCH/7zip/7za
        cd /tmp/7z
        cp DOC/License.txt $LIB_PATH/$ARCH/7zip/license-7zip.txt
        chmod -x $LIB_PATH/$ARCH/7zip/license-7zip.txt
        cd $NZBGET_ROOT
        rm -rf /tmp/7z
    fi
}

build_unrar_version()
{
    UNRAR_VERSION=$1
    if [ "$UNRAR_VERSION" == "6" ]; then
        UNRARSRC=https://www.rarlab.com/rar/unrarsrc-$UNRAR6_VERSION.tar.gz
    else
        UNRARSRC=https://www.rarlab.com/rar/unrarsrc-$UNRAR7_VERSION.tar.gz
    fi
    echo "Building unrar version $UNRAR_VERSION for $ARCH"
    curl -o /tmp/unrar.tar.gz $UNRARSRC
    cd /tmp
    tar zxf unrar.tar.gz
    rm unrar.tar.gz
    cd unrar
    sed "s|^CXX=.*|CXX=$CXX|" -i makefile
    sed "s|^AR=.*|AR=$AR|" -i makefile
    sed "s|^STRIP=.*|STRIP=$STRIP|" -i makefile
    if [ "$PLATFORM" == "android" ] ; then
        if [ "$UNRAR_VERSION" == "6" ]; then
            sed 's:^#if defined(_EMX) || defined (__VMS)$:#if defined(_EMX) || defined (__VMS) || defined (__ANDROID__):' -i consio.cpp
        else
            sed 's:#ifdef __VMS$:#if defined (__VMS) || defined (__ANDROID__):' -i consio.cpp
        fi
        sed 's:^#define USE_LUTIMES$:#undef USE_LUTIMES:' -i os.hpp
    fi
    # some unrar7 optimizations
    if [ "$UNRAR_VERSION" == "7" ]; then
        sed "s|LDFLAGS=-pthread|LDFLAGS=-pthread -static|" -i makefile
        case $ARCH in
            x86_64)
                sed "s|CXXFLAGS=-march=native|CXXFLAGS=-march=x86-64|" -i makefile
                ;;
            aarch64)
                sed "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv8-a+crypto+crc|" -i makefile
                ;;
            armhf)
                sed "s|CXXFLAGS=-march=native|CXXFLAGS=-march=armv7-a|" -i makefile
                ;;
            *)
                sed "s|CXXFLAGS=-march=native |CXXFLAGS=|" -i makefile
                ;;
        esac
    else
        sed "s|^LDFLAGS=.*|LDFLAGS=-static|" -i makefile
        sed "s|^CXXFLAGS=.*|CXXFLAGS=-std=c++11 -O2|" -i makefile
    fi
    make clean
    make -j $COREX
    mkdir -p $LIB_PATH/$ARCH/unrar
    if [ "$UNRAR_VERSION" == "6" ]; then
        cp unrar $LIB_PATH/$ARCH/unrar/unrar
        cp license.txt $LIB_PATH/$ARCH/unrar/license-unrar.txt
    else
        cp unrar $LIB_PATH/$ARCH/unrar/unrar7
        cp license.txt $LIB_PATH/$ARCH/unrar/license-unrar7.txt
    fi
    rm -rf /tmp/unrar
    cd $NZBGET_ROOT
}

build_unrar()
{
    if [ ! -d "$LIB_PATH/$ARCH/unrar" ]; then
        for UNRAR_VERSION in 6 7; do
            build_unrar_version $UNRAR_VERSION
        done
    fi
}

build_bin()
{
    # clean nzbget cmake variables
    unset LIBS
    unset INCLUDES

    # toolchain variables
    export ARCH=$ARCH
    case $ARCH in
        armel)
            export HOST="arm-buildroot-linux-musleabi"
            CMAKE_SYSTEM_PROCESSOR="arm"
            ;;
        armhf)
            export HOST="arm-buildroot-linux-musleabihf"
            CMAKE_SYSTEM_PROCESSOR="arm"
            ;;
        aarch64)
            export HOST="aarch64-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="aarch64"
            ;;
        i686)
            export HOST="i686-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="i686"
            ;;
        x86_64)
            export HOST="x86_64-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="x86_64"
            ;;
        riscv64)
            export HOST="riscv64-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="riscv64"
            ;;
        mipseb)
            export HOST="mips-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="mips"
            ;;
        mipsel)
            export HOST="mipsel-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="mips"
            ;;
        ppc6xx)
            export HOST="powerpc-buildroot-linux-musl"
            CMAKE_SYSTEM_PROCESSOR="powerpc"
            ;;
        ppc500)
            export HOST="powerpc-buildroot-linux-uclibcspe"
            CMAKE_SYSTEM_PROCESSOR="powerpc"
            ;;
        i686-ndk)
            export HOST="i686-linux-android"
            CMAKE_SYSTEM_PROCESSOR="i686"
            ;;
        x86_64-ndk)
            export HOST="x86_64-linux-android"
            CMAKE_SYSTEM_PROCESSOR="x86_64"
            ;;
        armhf-ndk)
            export HOST="arm-linux-androideabi"
            CMAKE_SYSTEM_PROCESSOR="arm"
            ;;
        aarch64-ndk)
            export HOST="aarch64-linux-android"
            CMAKE_SYSTEM_PROCESSOR="aarch64"
            ;;
    esac


    case $PLATFORM in
        android)
            TOOLCHAIN_PATH=$BUILDROOT_HOME/android
            unset CPP
            export CC="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-clang"
            export CXX="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-clang++"
            export AR="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-ar"
            export STRIP="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-strip"
            ;;
        *)
            TOOLCHAIN_PATH=$BUILDROOT_HOME/buildroot
            export CC="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-gcc"
            export CPP="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-cpp"
            export CXX="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-g++"
            export AR="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-ar"
            export STRIP="$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST-strip"
            ;;
    esac

    # clean build flags
    export CXXFLAGS="-Os"
    export CPPFLAGS="-Os"
    export LDFLAGS=""
    export NZBGET_INCLUDES="$TOOLCHAIN_PATH/$ARCH/output/staging/usr/include/;"

    build_lib "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-$NCURSES_VERSION.tar.gz"
    build_lib "https://zlib.net/zlib-$ZLIB_VERSION.tar.gz"
    build_lib "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.12.4/libxml2-v$LIBXML2_VERSION.tar.gz"
    build_lib "https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-$OPENSSL_VERSION.tar.gz"
    build_lib "https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-$BOOST_VERSION.tar.gz"

    build_7zip
    build_unrar

    case $PLATFORM in
        android)
            export LIBS="$LDFLAGS -lxml2 -lboost_json -lz -lssl -lcrypto -lncursesw -latomic"
            CMAKE_EXTRA_ARGS="-DCOMPILER=clang"
            ;;
        *)
            export LIBS="$LDFLAGS -lxml2 -lrt -lboost_json -lz -lssl -lcrypto -lncursesw -latomic -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
            CMAKE_EXTRA_ARGS=""
            ;;
    esac
    export INCLUDES="$NZBGET_INCLUDES"

    unset CXXFLAGS
    unset CPPFLAGS
    unset LDFLAGS

    if [ "$CONFIG" == "debug" ]; then
        CMAKE_BUILD_TYPE="Debug"
    else
        CMAKE_BUILD_TYPE="Release"
    fi

    mkdir -p $OUTPUTDIR/$ARCH
    cmake -S . -B $OUTPUTDIR/$ARCH \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=$CMAKE_SYSTEM_PROCESSOR \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake \
        -DTOOLCHAIN_PREFIX=$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST \
        -DENABLE_STATIC=ON \
        -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
        -DVERSION_SUFFIX=$VERSION_SUFFIX \
        -DCMAKE_INSTALL_PREFIX=$NZBGET_ROOT/$OUTPUTDIR/install/$ARCH \
        $CMAKE_EXTRA_ARGS
    BUILD_STATUS=""
    cmake --build $OUTPUTDIR/$ARCH -j $COREX 2>$OUTPUTDIR/$ARCH/build.log || BUILD_STATUS=$?
    if [ ! -z $BUILD_STATUS ]; then
        tail -20 $OUTPUTDIR/$ARCH/build.log
        exit 1
    fi
    cmake --install $OUTPUTDIR/$ARCH

    cd $OUTPUTDIR/install/$ARCH

    mkdir -p nzbget
    mv bin/nzbget nzbget/nzbget
    mv share/nzbget/webui nzbget
    mv share/nzbget/doc/* nzbget

    CONFTEMPLATE=nzbget/webui/nzbget.conf.template
    mv etc/nzbget.conf $CONFTEMPLATE

    # adjusting nzbget.conf
    sed 's|^MainDir=.*|MainDir=${AppDir}/downloads|' -i $CONFTEMPLATE
    sed 's|^DestDir=.*|DestDir=${MainDir}/completed|' -i $CONFTEMPLATE
    sed 's|^InterDir=.*|InterDir=${MainDir}/intermediate|' -i $CONFTEMPLATE
    sed 's|^WebDir=.*|WebDir=${AppDir}/webui|' -i $CONFTEMPLATE
    sed 's|^ScriptDir=.*|ScriptDir=${AppDir}/scripts|' -i $CONFTEMPLATE
    sed 's|^LogFile=.*|LogFile=${MainDir}/nzbget.log|' -i $CONFTEMPLATE
    sed 's|^ConfigTemplate=.*|ConfigTemplate=${AppDir}/webui/nzbget.conf.template|' -i $CONFTEMPLATE
    sed 's|^AuthorizedIP=.*|AuthorizedIP=127.0.0.1|' -i $CONFTEMPLATE

    rm -rf etc bin share
    tar -czf $BASENAME-bin-$PLATFORM-$ARCH_SHORT$SUFFIX.tar.gz nzbget
    mv *.tar.gz $NZBGET_ROOT/$OUTPUTDIR
    cd $NZBGET_ROOT
    rm -rf $OUTPUTDIR/$ARCH
    rm -rf $OUTPUTDIR/install
}

build_installer()
{
    filter_archs
    if [ "$PLATFORM_ARCHS" == "" ]; then return; fi

    echo "Creating installer for $PLATFORM $CONFIG..."

    cd $OUTPUTDIR
    # checking if all targets exists
    for ARCH in $PLATFORM_ARCHS; do
        get_short_arch
        ALLEXISTS="yes"
        if [ ! -f $BASENAME-bin-$PLATFORM-$ARCH_SHORT$SUFFIX.tar.gz ]; then
            echo "Could not find $BASENAME-bin-$PLATFORM-$ARCH_SHORT$SUFFIX.tar.gz"
            ALLEXISTS="no"
        fi
    done

    if [ "$ALLEXISTS" == "no" ]; then
        exit 1;
    fi

    echo "Unpacking targets..."
    rm -r -f nzbget
    for ARCH in $PLATFORM_ARCHS; do
        get_short_arch
        ALLEXISTS="yes"
        tar zxf $BASENAME-bin-$PLATFORM-$ARCH_SHORT$SUFFIX.tar.gz
        mv nzbget/nzbget nzbget/nzbget-$ARCH_SHORT
        cp $LIB_PATH/$ARCH/unrar/unrar nzbget/unrar-$ARCH_SHORT
        cp $LIB_PATH/$ARCH/unrar/unrar7 nzbget/unrar7-$ARCH_SHORT
        cp $LIB_PATH/$ARCH/7zip/7za nzbget/7za-$ARCH_SHORT
    done

    # adjusting nzbget.conf
    sed 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' -i nzbget/webui/nzbget.conf.template
    sed 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' -i nzbget/webui/nzbget.conf.template
    sed 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' -i nzbget/webui/nzbget.conf.template
    sed 's:^CertCheck=.*:CertCheck=yes:' -i nzbget/webui/nzbget.conf.template

    INSTFILE=$BASENAME-bin-$PLATFORM$SUFFIX.run

    echo "Building installer package..."
    cp $NZBGET_ROOT/linux/installer.sh $INSTFILE
    cp $NZBGET_ROOT/linux/package-info.json nzbget/webui
    cp $NZBGET_ROOT/linux/install-update.sh nzbget
    cp $NZBGET_ROOT/pubkey.pem nzbget
    cp $LIB_PATH/$ARCH/unrar/license-unrar.txt nzbget
    cp $LIB_PATH/$ARCH/7zip/license-7zip.txt nzbget
    curl -o nzbget/cacert.pem https://curl.se/ca/cacert.pem

    # adjusting update config file
    sed "s:linux:$PLATFORM:" -i nzbget/webui/package-info.json
    sed "s:linux:$PLATFORM:" -i nzbget/install-update.sh

    # creating payload
    cd nzbget
    tar czf - * > ../$INSTFILE.data
    cd ..

    # creating installer script
    sed "s:^TITLE=$:TITLE=\"$BASENAME$SUFFIX\":" -i $INSTFILE
    sed "s:^PLATFORM=$:PLATFORM=\"$PLATFORM\":" -i $INSTFILE
    DISTTARGETS=""
    for ARCH in $PLATFORM_ARCHS; do
        get_short_arch
        DISTTARGETS=`echo "$DISTTARGETS $ARCH_SHORT" | xargs`
    done
    sed "s:^DISTARCHS=$:DISTARCHS=\"$DISTTARGETS\":" -i $INSTFILE

    MD5=`md5sum "$INSTFILE.data" | cut -b-32`
    sed "s:^MD5=$:MD5=\"$MD5\":" -i $INSTFILE

    PAYLOAD=`stat -c%s "$INSTFILE.data"`
    PAYLOADLEN=${#PAYLOAD}

    HEADER=`stat -c%s "$INSTFILE"`
    HEADERLEN=${#HEADER}
    HEADER=`expr $HEADER + $HEADERLEN + $PAYLOADLEN`

    TOTAL=`expr $HEADER + $PAYLOAD`
    TOTALLEN=${#TOTAL}

    HEADER=`expr $HEADER - $PAYLOADLEN + $TOTALLEN`
    TOTAL=`expr $TOTAL - $PAYLOADLEN + $TOTALLEN`

    sed "s:^HEADER=$:HEADER=$HEADER:" -i $INSTFILE
    sed "s:^TOTAL=$:TOTAL=$TOTAL:" -i $INSTFILE

    # attaching payload
    cat $INSTFILE.data >> $INSTFILE
    rm $INSTFILE.data
    chmod +x $INSTFILE

    rm -r nzbget
    cd $NZBGET_ROOT
}

NZBGET_ROOT=$PWD

# cleanup shared and build directories
mkdir -p $OUTPUTDIR
rm -rf $OUTPUTDIR/*

parse_args $@
print_config

# version handling
VERSION=$(grep "set(VERSION " CMakeLists.txt | cut -d '"' -f 2)
VERSION_SUFFIX=""
if [ "$TESTING" == "yes" ]; then
    VERSION_SUFFIX="-testing-$(date '+%Y%m%d')"
fi
BASENAME="nzbget-$VERSION$VERSION_SUFFIX"

# build binary packages
if [[ $OUTPUTS == *"bin"* ]]; then
    for CONFIG in $CONFIGS; do
        construct_suffix
        for PLATFORM in $PLATFORMS; do
            filter_archs
            for ARCH in $PLATFORM_ARCHS; do
                get_short_arch
                build_bin
            done
        done
    done
fi

# build installers
if [[ $OUTPUTS == *"installer"* ]]; then
    for PLATFORM in $PLATFORMS; do
        for CONFIG in $CONFIGS; do
            construct_suffix
            build_installer
        done
    done
fi
