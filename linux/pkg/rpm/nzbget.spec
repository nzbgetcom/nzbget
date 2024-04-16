Name: nzbget
Version: $RPM_VERSION
Release: 1
Group: Applications/Internet
Provides: nzbget
Requires: systemd
Recommends: p7zip unrar >= 6
Summary: Command-line based binary newsgrabber for nzb files, written in C++
License: GPLv2

%description
NZBGet is a command-line based binary newsgrabber for nzb files,
written in C++.
It supports client/server mode, automatic par-check/-repair,
unpack and web-interface. NZBGet requires low system resources.

%define __spec_install_pre /bin/true

%prep

%build

%install

%files
/usr/bin/nzbget
/usr/share/doc/nzbget/
/usr/share/nzbget/
/usr/lib/systemd/system/nzbget.service

%post
if [ $1 == 1 ]; then # install
    NZBGET_HOME=/var/lib/nzbget
    NZBGET_USER=nzbget
    NZBGET_GROUP=nzbget
    NZBGET_NAME="NZBGet daemon user"
    
    # create user to avoid running nzbget as root
    # create group if not existing
    if ! getent group | grep -q "^$NZBGET_GROUP:"; then
        echo -n "Adding group $NZBGET_GROUP.."
        groupadd --system $NZBGET_GROUP 1>/dev/null 2>&1 || true
        echo "..done"
    fi
    
    # create nzbget home if not existing
    test -d $NZBGET_HOME || mkdir $NZBGET_HOME
    
    # create nzbget user if not existing
    if ! getent passwd | grep -q "^$NZBGET_USER:"; then
        echo -n "Adding system user $NZBGET_USER.."
        adduser --system \
                --gid $NZBGET_GROUP \
                --no-create-home \
                --home $NZBGET_HOME \
                --shell /sbin/nologin \
                $NZBGET_USER 1>/dev/null 2>&1 || true
        echo "..done"
    fi

    # adjust passwd entry
    usermod -c "$NZBGET_NAME" -d $NZBGET_HOME -g $NZBGET_GROUP $NZBGET_USER >/dev/null

    # copy default config if not exist
    if [ ! -f /var/lib/nzbget/nzbget.conf ]; then
        cp /usr/share/nzbget/nzbget.conf $NZBGET_HOME/nzbget.conf
    fi
    
    mkdir -p $NZBGET_HOME/scripts

    # adjust file and directory permissions
    chown -R $NZBGET_USER:$NZBGET_GROUP $NZBGET_HOME
    chmod u=rwx,g=rxs,o= $NZBGET_HOME    
fi

# reload systemd
systemctl --system daemon-reload >/dev/null || true
# enable nzbget service
systemctl enable nzbget >/dev/null || true

# start or restart nzbget service
if [ $1 == 1 ]; then # install
    systemctl start nzbget >/dev/null || true
elif [ $1 == 2 ]; then # upgrade
    systemctl restart nzbget >/dev/null || true
fi

%preun
echo $1
if [ $1 == 0 ]; then # remove
    # stop and disable nzbget service
    systemctl stop nzbget >/dev/null || true
    systemctl disable nzbget >/dev/null || true    
fi

%postun
if [ $1 == 0 ]; then # remove
    # reload systemd
    systemctl --system daemon-reload >/dev/null || true
    # purge nzbget home directory
    rm -rf /var/lib/nzbget/scripts
    rm -rf /var/lib/nzbget/tmp
    rm -rf /var/lib/nzbget/nzbget.conf
    rm -rf /var/lib/nzbget/downloads/intermediate
    rm -rf /var/lib/nzbget/downloads/queue
    # remove completed only if empty
    if [ -d /var/lib/nzbget/downloads/completed ]; then
        if [ -z "$(ls -A var/lib/nzbget/downloads/completed)" ]; then
            rm -rf /var/lib/nzbget/downloads
        fi
    fi
fi
