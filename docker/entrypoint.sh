#!/usr/bin/env sh
set -e

# create downloads if not exist
if [ ! -d /downloads ]; then
    mkdir -p /downloads
fi

# delete lock file if found
if [ -f /downloads/nzbget.lock ]; then
    rm /downloads/nzbget.lock
fi

# change userid and groupid
PUID=${PUID:-1000}
PGID=${PGID:-1000}
groupmod -o -g "$PGID" users >/dev/null
usermod -o -u "$PUID" user >/dev/null

# create default config if not exist
if [ ! -f /config/nzbget.conf ]; then
    cp /app/nzbget/share/nzbget/nzbget.conf /config/nzbget.conf
    chown user:users /config/nzbget.conf
fi

# create scripts dir
if [ ! -d /downloads/scripts ]; then
    mkdir -p /downloads/scripts
    chown user:users /downloads/scripts
fi

# parse env vars to options
OPTIONS=""
if [ ! -z "${NZBGET_USER}" ]; then
  OPTIONS="${OPTIONS}-o ControlUsername=${NZBGET_USER} "
fi
if [ ! -z "${NZBGET_PASS}" ]; then
  OPTIONS="${OPTIONS}-o ControlPassword=${NZBGET_PASS} "
fi

chown user:users /config || CONFIG_CHOWN_STATUS=$?
if [ ! -z $CONFIG_CHOWN_STATUS ]; then
  echo "*** Could not set permissions on /config ; this container may not work as expected ***"
fi

chown user:users /downloads || DOWNLOADS_CHOWN_STATUS=$?
if [ ! -z $DOWNLOADS_CHOWN_STATUS ]; then
  echo "*** Could not set permissions on /downloads ; this container may not work as expected ***"
fi

su -p user -c "/app/nzbget/nzbget -s -c /config/nzbget.conf -o OutputMode=log ${OPTIONS}"
