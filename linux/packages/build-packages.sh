#!/bin/sh

INSTALLER=nzbget-latest-testing-bin-linux.run
if [ ! -f "$INSTALLER" ]; then
    wget https://github.com/nzbgetcom/nzbget/releases/download/testing/$INSTALLER
fi

# extract version
VERSION=$(bash nzbget-latest-testing-bin-linux.run --help | grep 'Installer for' | cut -d ' ' -f 3 | sed -r 's/nzbget-//')
RPM_VERSION=${VERSION//-/}

for ARCH in x86_64 i686 armel armhf aarch64; do
    bash $INSTALLER --arch "$ARCH" --destdir "$PWD/$ARCH" --silent    
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

    # build debian package
    CONTENTS="$PWD/$ARCH/CONTENTS/"
    SHARE="$CONTENTS/usr/local/share/nzbget/"
    SHAREDOC="$CONTENTS/usr/local/share/doc/nzbget/"
    mkdir -p $CONTENTS/DEBIAN/
    eval "echo \"$(cat deb/DEBIAN/control)\"" > "$CONTENTS/DEBIAN/control"
    mkdir -p $CONTENTS/usr/local/bin
    mkdir -p $CONTENTS/usr/local/share/doc/nzbget
    mkdir -p $CONTENTS/usr/local/share/nzbget
    cp $PWD/$ARCH/nzbget $CONTENTS/usr/local/bin
    for DOCFILE in ChangeLog COPYING INSTALLATION.md; do
        cp $PWD/$ARCH/$DOCFILE $SHAREDOC
    done
    mv $PWD/$ARCH/scripts $SHARE
    mv $PWD/$ARCH/webui $SHARE
    cp $PWD/$ARCH/nzbget.conf $SHARE
    cp $PWD/$ARCH/cacert.pem $SHARE
    
    # tweak nzbget.conf
    sed -i \
        -e "s#^MainDir=.*#MainDir=~/downloads#g" \
        -e "s#^ScriptDir=.*#ScriptDir=/usr/local/share/nzbget/scripts#g" \
        -e "s#^WebDir=.*#WebDir=/usr/local/share/nzbget/webui#g" \
        -e "s#^ConfigTemplate=.*#ConfigTemplate=/usr/local/share/nzbget/nzbget.conf#g" \
        -e "s#^UnrarCmd=.*#UnrarCmd=unrar#g" \
        -e "s#^SevenZipCmd=.*#SevenZipCmd=7zz#g" \
        -e "s#^CertStore=.*#CertStore=/usr/local/share/nzbget/cacert.pem#g" \
        -e "s#^CertCheck=.*#CertCheck=yes#g" \
        -e "s#^DestDir=.*#DestDir=$\{MainDir\}/completed#g" \
        -e "s#^InterDir=.*#InterDir=$\{MainDir\}/intermediate#g" \
        -e "s#^LogFile=.*#LogFile=$\{MainDir\}/nzbget.log#g" \
        -e "s#^AuthorizedIP=.*#AuthorizedIP=127.0.0.1#g" \
        ${SHARE}nzbget.conf
    find $PWD/$ARCH/ -maxdepth 1 -type f -delete
    dpkg-deb --build $CONTENTS $PWD/nzbget-$VERSION-$DPKG_ARCH.deb    

    # make rpm package
    # prepare spec
    eval "echo \"$(cat rpm/nzbget.spec)\"" > "$ARCH/nzbget.spec"

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
    cp $PWD/$ARCH/RPMS/$RPM_ARCH/*.rpm .
    rm -rf $PWD/$ARCH
done