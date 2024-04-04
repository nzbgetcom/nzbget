#!/bin/sh

# stop on error
set -e

NZBGET_ROOT=$PWD

# installer - first param
# if empty - find installer file in artifacts dir
INSTALLER="$1"
if [ -z $INSTALLER ]; then
    INSTALLER=$(find $NZBGET_ROOT/nzbget-linux-installers/ -name *linux.run 2>/dev/null) || true
fi
if [ -z $INSTALLER ]; then
    echo "Cannot find linux installer file. Exitig."
    exit 1
fi

# config variables
DEB=yes
RPM=no
ARCHS="i686 x86_64 armel armhf aarch64"

# prepare directories
mkdir -p build
rm -rf build/*
cd build
mkdir -p deb
mkdir -p rpm

# extract version
VERSION=$(bash "$INSTALLER" --help | grep 'Installer for' | cut -d ' ' -f 3 | sed -r 's/nzbget-//')
RPM_VERSION=${VERSION//-/}

for ARCH in $ARCHS; do
    bash "$INSTALLER" --arch "$ARCH" --destdir "$PWD/$ARCH" --silent    
    case $ARCH in
        i686)
            DPKG_ARCH=i386
            RPM_ARCH=i386
            ;;
        x86_64)
            DPKG_ARCH=amd64
            RPM_ARCH=x86_64
            ;;
        aarch64)
            DPKG_ARCH=arm64
            RPM_ARCH=aarch64
            ;;
        armhf)
            DPKG_ARCH=armhf
            RPM_ARCH=armv7l
            ;;
        armel)
            DPKG_ARCH=armel
            RPM_ARCH=armv6l
            ;;
        *)
            DPKG_ARCH=$ARCH
            RPM_ARCH=$ARCH
            ;;
    esac

    # prepare files
    CONTENTS="$PWD/$ARCH/CONTENTS/"
    SHARE="$CONTENTS/usr/share/nzbget/"
    SHAREDOC="$CONTENTS/usr/share/doc/nzbget/"
    mkdir -p $CONTENTS/usr/bin
    mkdir -p $CONTENTS/usr/share/doc/nzbget
    mkdir -p $CONTENTS/usr/share/nzbget
    cp $PWD/$ARCH/nzbget $CONTENTS/usr/bin
    for DOCFILE in ChangeLog COPYING; do
        cp $PWD/$ARCH/$DOCFILE $SHAREDOC
    done
    mv $PWD/$ARCH/scripts $SHARE
    mv $PWD/$ARCH/webui $SHARE
    cp $PWD/$ARCH/nzbget.conf $SHARE
    cp $PWD/$ARCH/cacert.pem $SHARE

    # tweak nzbget.conf
    sed -i \
        -e "s|^MainDir=.*|MainDir=/var/lib/nzbget/downloads|g" \
        -e "s|^ScriptDir=.*|ScriptDir=/var/lib/nzbget/scripts|g" \
        -e "s|^WebDir=.*|WebDir=/usr/share/nzbget/webui|g" \
        -e "s|^TempDir=.*|TempDir=/var/lib/nzbget/tmp|g" \
        -e "s|^ConfigTemplate=.*|ConfigTemplate=/usr/share/nzbget/nzbget.conf|g" \
        -e "s|^UnrarCmd=.*|UnrarCmd=unrar|g" \
        -e "s|^SevenZipCmd=.*|SevenZipCmd=7zz|g" \
        -e "s|^CertStore=.*|CertStore=/usr/share/nzbget/cacert.pem|g" \
        -e "s|^CertCheck=.*|CertCheck=yes|g" \
        -e "s|^DestDir=.*|DestDir=$\{MainDir\}/completed|g" \
        -e "s|^InterDir=.*|InterDir=$\{MainDir\}/intermediate|g" \
        -e "s|^AuthorizedIP=.*|AuthorizedIP=127.0.0.1|g" \
        -e 's|^UpdateCheck=.*|UpdateCheck=none|g' \
        -e 's|^UMask=.*|UMask=0002|g' \
        "${SHARE}nzbget.conf"

    # build debian package
    if [ "$DEB" == "yes" ]; then
        mkdir -p $CONTENTS/DEBIAN/
        cp $NZBGET_ROOT/linux/pkg/deb/DEBIAN/* "$CONTENTS/DEBIAN/"
        cp -r $NZBGET_ROOT/linux/pkg/deb/CONTENTS "$PWD/$ARCH/"
        eval "echo \"$(cat ../linux/pkg/deb/DEBIAN/control)\"" > "$CONTENTS/DEBIAN/control"        
        mkdir -p "$CONTENTS/lib/systemd/system/"
        cp ../linux/pkg/nzbget.service "$CONTENTS/lib/systemd/system/"
        # fix permissions
        chmod -R u+rwX,go+rX,go-w "$CONTENTS/lib"
        chmod -R u+rwX,go+rX,go-w "$CONTENTS/usr"
        # remove unneeded files
        find $PWD/$ARCH/ -maxdepth 1 -type f -delete        
        fakeroot dpkg-deb --build $CONTENTS $PWD/deb/nzbget-$VERSION-$DPKG_ARCH.deb
    fi

    # build rpm package
    if [ "$RPM" == "yes" ]; then
        # prepare spec
        eval "echo \"$(cat ../linux/pkg/rpm/nzbget.spec)\"" > "$ARCH/nzbget.spec"

        # prepare directories
        if [ "$ARCH" == "armel" ] || [ "$ARCH" == "armhf" ]; then
            ARCH_PATH="arm"
        else
            ARCH_PATH=$RPM_ARCH
        fi
        RPM_SRC="$PWD/$ARCH/BUILDROOT/nzbget-$RPM_VERSION-1.$ARCH_PATH"
        mkdir -p $RPM_SRC
        cp -r $CONTENTS/usr/ $RPM_SRC    
        rpmbuild --define "_topdir $PWD/$ARCH" -bb $PWD/$ARCH/nzbget.spec --target $RPM_ARCH
        cp $PWD/$ARCH/RPMS/$RPM_ARCH/*.rpm rpm/
    fi

    rm -rf $PWD/$ARCH
done
