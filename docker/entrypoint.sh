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

# create scripts dir
mkdir -p /downloads/scripts

# change userid and groupid
PUID=${PUID:-1000}
PGID=${PGID:-1000}
groupmod -o -g "$PGID" users
usermod -o -u "$PUID" user

chown -R user:users /config
chown -R user:users /downloads
su -p user -c "/app/nzbget/nzbget -s -c /config/nzbget.conf -o OutputMode=log ${OPTIONS}"
