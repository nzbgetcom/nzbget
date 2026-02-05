#!/usr/bin/env sh
set -e

# create downloads if not exist
if [ ! -d /downloads ]; then
    mkdir -p /downloads
fi

# delete lock file if found
if [ -f /downloads/nzbget.lock ]; then
    rm /downloads/nzbget.lock
fi

# change userid and groupid
PUID=${PUID:-1000}
PGID=${PGID:-1000}
if [ "$PGID" != "$(id -g user)" ]; then
    groupmod -o -g "$PGID" users >/dev/null
fi
if [ "$PUID" != "$(id -u user)" ]; then
    usermod -o -u "$PUID" user >/dev/null
fi

# create default config if not exist
if [ ! -f /config/nzbget.conf ]; then
    cp /app/nzbget/share/nzbget/nzbget.conf /config/nzbget.conf
fi

# parse env vars to options
OPTIONS=""
if [ ! -z "${NZBGET_USER}" ]; then
    OPTIONS="${OPTIONS}-o ControlUsername=${NZBGET_USER} "
fi
if [ ! -z "${NZBGET_PASS}" ]; then
    OPTIONS="${OPTIONS}-o ControlPassword=${NZBGET_PASS} "
fi

if [ "$(id -u)" -eq 0 ]; then
    chown user:users /config/nzbget.conf

    chown user:users /config || CONFIG_CHOWN_STATUS=$?
    if [ ! -z $CONFIG_CHOWN_STATUS ]; then
        echo "*** Could not set permissions on /config ; this container may not work as expected ***"
    fi

    chown user:users /downloads || DOWNLOADS_CHOWN_STATUS=$?
    if [ ! -z $DOWNLOADS_CHOWN_STATUS ]; then
        echo "*** Could not set permissions on /downloads ; this container may not work as expected ***"
    fi

    exec su-exec user:users /app/nzbget/nzbget -s -c /config/nzbget.conf -o OutputMode=log ${OPTIONS}
else
    exec /app/nzbget/nzbget -s -c /config/nzbget.conf -o OutputMode=log ${OPTIONS}
fi
