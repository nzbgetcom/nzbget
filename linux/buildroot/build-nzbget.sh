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
                --host=$HOST \
                --prefix="$PWD/../$LIB"
                ;;
            "zlib")
                ./configure --static --prefix="$PWD/../$LIB"
                ;;
            "libxml2")
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
                perl Configure linux-$OPENSSL_ARCH \
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
                --prefix="$PWD/../$LIB"
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
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/libxml2/;"
    elif [ "$LIB" == "ncurses" ]; then
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/;$LIB_PATH/$ARCH/$LIB/include/ncurses/;"
    else
        export NZBGET_INCLUDES="$NZBGET_INCLUDES$LIB_PATH/$ARCH/$LIB/include/;"
    fi
    cd $NZBGET_ROOT
}

build_bin()
{
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
        export CXXFLAGS="-Os"
        export CPPFLAGS="-Os"
        export LDFLAGS=""
        export NZBGET_INCLUDES="$TOOLCHAIN_PATH/$ARCH/output/staging/usr/include/;"

        build_lib "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz"
        build_lib "https://zlib.net/zlib-1.3.1.tar.gz"
        build_lib "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.12.4/libxml2-v2.12.4.tar.gz"
        build_lib "https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-3.1.2.tar.gz"
        build_lib "https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz"

        # build_7zip
        # build_unrar

        export LIBS="$LDFLAGS -lxml2 -lrt -lboost_json -lz -lssl -lcrypto -lncurses -latomic -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
        export INCLUDES="$NZBGET_INCLUDES"
        unset CXXFLAGS
        unset CPPFLAGS
        unset LDFLAGS
    
        mkdir -p build/$ARCH
        cmake -S . -B build/$ARCH \
            -DCMAKE_SYSTEM_NAME=Linux \
            -DCMAKE_SYSTEM_PROCESSOR=arm \
            -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake \
            -DTOOLCHAIN_PREFIX=$TOOLCHAIN_PATH/$ARCH/output/host/usr/bin/$HOST \
            -DENABLE_STATIC=ON \
            -DCMAKE_INSTALL_PREFIX=$NZBGET_ROOT/build/install/$ARCH
        cmake --build build/$ARCH -j $COREX 2>/dev/null
        cmake --install build/$ARCH
        
        cd build/install/$ARCH
        
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
        tar -czf $BASENAME-bin-$PLATFORM-$ARCH$SUFFIX.tar.gz nzbget
        mv *.tar.gz $NZBGET_ROOT/build    
        cd $NZBGET_ROOT
        rm -rf build/$ARCH
        rm -rf build/install
    done
}

build_installer()
{
    echo "Creating installer for $PLATFORM $CONFIG..."

    cd $OUTPUTDIR

    # checking if all targets exists
    for TARGET in $TARGETS
    do
        ALLEXISTS="yes"
        if [ $TARGET != "dist" ]; then
            if [ ! -f $BASENAME-bin-$PLATFORM-$TARGET$SUFFIX.tar.gz ]; then
                echo "Could not find $BASENAME-bin-$PLATFORM-$TARGET$SUFFIX.tar.gz"
                ALLEXISTS="no"
            fi
        fi
    done

    if [ "$ALLEXISTS" == "no" ]; then
        exit 1;
    fi

    echo "Unpacking targets..."
    rm -r -f nzbget
    for TARGET in $TARGETS
    do
        ALLEXISTS="yes"
        if [ "$TARGET" != "dist" ]; then
            tar -xzf $BASENAME-bin-$PLATFORM-$TARGET$SUFFIX.tar.gz
            mv nzbget/nzbget nzbget/nzbget-$TARGET
            cp ../setup/unrar-$TARGET$PLATSUFF nzbget/unrar-$TARGET
            cp ../setup/7za-$TARGET$PLATSUFF nzbget/7za-$TARGET
        fi
    done

    # adjusting nzbget.conf
    sed 's:^UnrarCmd=unrar:UnrarCmd=${AppDir}/unrar:' -i nzbget/webui/nzbget.conf.template
    sed 's:^SevenZipCmd=7z:SevenZipCmd=${AppDir}/7za:' -i nzbget/webui/nzbget.conf.template
    sed 's:^CertStore=.*:CertStore=${AppDir}/cacert.pem:' -i nzbget/webui/nzbget.conf.template
    sed 's:^CertCheck=.*:CertCheck=yes:' -i nzbget/webui/nzbget.conf.template

    INSTFILE=$BASENAME-bin-$PLATFORM$SUFFIX.run

    echo "Building installer package..."
    cp $BUILDDIR/linux/installer.sh $INSTFILE
    cp $BUILDDIR/linux/package-info.json nzbget/webui
    cp $BUILDDIR/linux/install-update.sh nzbget
    cp $BUILDDIR/pubkey.pem nzbget
    cp ../setup/license-unrar.txt nzbget
    cp ../setup/license-7zip.txt nzbget
    cp ../setup/cacert.pem nzbget

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
    DISTTARGETS="${TARGETS/dist/}"
    DISTTARGETS=`echo "$DISTTARGETS" | xargs`
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
}

# cleanup shared and build directories
mkdir -p build
rm -rf build/*
NZBGET_ROOT=$PWD

VERSION=$(grep "set(VERSION " CMakeLists.txt | cut -d '"' -f 2)
BASENAME="nzbget-$VERSION"
PLATFORM="linux"

build_bin
build_installer