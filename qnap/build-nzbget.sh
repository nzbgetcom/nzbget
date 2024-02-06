#!/bin/bash
set -e

# config variables
QNAP_ROOT=/qnap
TOOLCHAIN_PATH=$QNAP_ROOT/toolchain
LIB_SRC_PATH=$QNAP_ROOT/source
LIB_PATH=$QNAP_ROOT/lib
ALL_ARCHS="i686 x86_64 aarch64"
UNRAR_VERSION=6.2.12
P7ZIP_VERSION=16.02
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
                export > /tmp/export.txt
                ./configure --with-termlib --without-progs --host=$HOST --prefix="$PWD/../$LIB"
                ;;
            "zlib")
                ./configure --static --prefix="$PWD/../$LIB"
                ;;
            "libxml2")
                ./autogen.sh --host=$HOST --enable-static --disable-shared --without-python --prefix="$PWD/../$LIB"
                ;;
            "musl")
                ./configure --prefix="$PWD/../$LIB"
                ;;
            "openssl")
                case $ARCH in
                    "i686")
                        OPENSSL_ARCH=generic32
                        ;;
                    *)
                        OPENSSL_ARCH=$ARCH
                        ;;
                esac                
                perl Configure linux-$OPENSSL_ARCH no-shared --prefix="$PWD/../$LIB"
                ;;
            "boost")
                ./bootstrap.sh --with-libraries=json --prefix="$PWD/../$LIB"
                echo "using gcc : qnap : $CXX ; " >>  project-config.jam
                ./b2 --toolset=gcc-qnap cxxstd=14 link=static runtime-link=static install
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
        export LDFLAGS="$LDFLAGS -L$LIB_PATH/$ARCH/$LIB/lib"
    fi
    cd $NZBGET_ROOT
}

build_unrar()
{
    if [ ! -d "$LIB_PATH/$ARCH/unrar" ]; then
        curl -o /tmp/unrar.tar.gz https://www.rarlab.com/rar/unrarsrc-$UNRAR_VERSION.tar.gz
        cd /tmp
        tar zxf unrar.tar.gz
        rm unrar.tar.gz
        cd unrar
        sed "s|^CXX=.*|CXX=$CXX|" -i makefile
        sed "s|^AR=.*|AR=$AR|" -i makefile
        sed "s|^STRIP=.*|STRIP=$STRIP|" -i makefile
        sed "s|^LDFLAGS=.*|LDFLAGS=-lm -lpthread|" -i makefile
        sed "s|^CXXFLAGS=.*|CXXFLAGS=-std=c++11 -O2|" -i makefile
        make clean
        make -j $COREX
        mkdir -p $LIB_PATH/$ARCH/unrar
        cp unrar $LIB_PATH/$ARCH/unrar/unrar
        cp license.txt $LIB_PATH/$ARCH/unrar/license-unrar.txt
        rm -rf /tmp/unrar
        cd $NZBGET_ROOT
    fi    
}

build_7zip()
{
    if [ ! -d "$LIB_PATH/$ARCH/7zip" ]; then
        curl -o /tmp/p7zip.tar.bz2 -lL https://sourceforge.net/projects/p7zip/files/p7zip/${P7ZIP_VERSION}/p7zip_${P7ZIP_VERSION}_src_all.tar.bz2    
        cd /tmp
        tar jxf p7zip.tar.bz2
        rm p7zip.tar.bz2
        cd p7zip_$P7ZIP_VERSION
        rm makefile.machine
        cp makefile.linux_any_cpu_gcc_4.X makefile.machine
        sed "s|^CXX=.*|CXX=$CXX|" -i makefile.machine
        sed "s|^CC=.*|CC=$CC|" -i makefile.machine
        make clean
        make -j $COREX
        mkdir -p $LIB_PATH/$ARCH/7zip
        cp bin/7za $LIB_PATH/$ARCH/7zip/7za
        cp DOC/License.txt $LIB_PATH/$ARCH/7zip/license-7zip.txt
        rm -rf /tmp/p7zip_$P7ZIP_VERSION
        cd $NZBGET_ROOT
    fi
}

# cleanup shared and build directories
rm -rf $QNAP_ROOT/nzbget
cp -r qnap/package $QNAP_ROOT/nzbget
NZBGET_ROOT=$PWD

# extract version and correct version in qpkg.cfg
VERSION=$(cat configure.ac | grep AC_INIT | cut -d , -f 2 | xargs)
sed "s|^QPKG_VER=.*|QPKG_VER=\"$VERSION\"|" -i $QNAP_ROOT/nzbget/qpkg.cfg

for ARCH in $ALL_ARCHS; do

    # toolchain variables
    export ARCH=$ARCH
    export HOST="$ARCH-QNAP-linux-gnu"
    export CC="$TOOLCHAIN_PATH/$ARCH/cross-tools/bin/$HOST-gcc"
    export CPP="$TOOLCHAIN_PATH/$ARCH/cross-tools/bin/$HOST-cpp"
    export CXX="$TOOLCHAIN_PATH/$ARCH/cross-tools/bin/$HOST-g++"
    export AR="$TOOLCHAIN_PATH/$ARCH/cross-tools/bin/$HOST-ar"
    export STRIP="$TOOLCHAIN_PATH/$ARCH/cross-tools/bin/$HOST-strip"

    # clean build flags
    export CXXFLAGS=""
    export CPPFLAGS=""
    export LDFLAGS=""
    export LIBS=""

    case $ARCH in
        "i686")
            QPKG_ARCH=x86
            ;;
        "aarch64")
            QPKG_ARCH=arm_64
            ;;
        *)
            QPKG_ARCH=$ARCH
            ;;
    esac

    build_lib "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz"
    build_lib "https://zlib.net/zlib-1.3.1.tar.gz"
    build_lib "https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.12.4/libxml2-v2.12.4.tar.gz"
    build_lib "https://github.com/openssl/openssl/releases/download/openssl-3.1.2/openssl-3.1.2.tar.gz"
    build_lib "https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz"

    build_7zip
    build_unrar

    autoreconf --install

    # we want to static link to all libs except libc
    export CXXFLAGS="$CXXFLAGS -std=c++14 -O2"
    export LIBS="-lncurses -lxml2 -lz -lm -lcrypto -ldl -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
    ./configure --disable-cpp-check --disable-dependency-tracking --host=$HOST
    make clean
    make -j $COREX
    
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

        cp $CONFTEMPLATE nzbget/nzbget.conf        
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

# rename qpkgs
for FILE in $QNAP_ROOT/nzbget/build/*.qpkg; do
    [ -f $FILE ] || continue
    NEW_FILE=${FILE/.qpkg/_native.qpkg}
    mv $FILE $NEW_FILE
done
