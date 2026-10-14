#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

if test "$NZBUP_PROCESSID" = ""; then
    echo "This file is not supposed to be executed directly. To update NZBGet please choose Settings -> SYSTEM -> Check for update in the web-interface."
    exit 1
fi

INSTALLERCFG=`cat "$NZBOP_APPDIR/installer.cfg" 2>/dev/null`
if test "$INSTALLERCFG" = ""; then
    echo "[ERROR] File \"installer.cfg\" is missing in the installation directory. Please reinstall NZBGet."
    exit 1
fi

echo "Downloading version information..."
UPDATE_INFO_LINK=`cat "$NZBOP_APPDIR/webui/package-info.json" | sed -n 's/^.*update-info-link.*: "\(.*\)".*/\1/p'`
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" "$UPDATE_INFO_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

if test "$NZBUP_BRANCH" = "TESTING"; then
    VER_FIELD="testing-version"
    DNL_FIELD="testing-download"
    SIG_FIELD="testing-signature"
elif test "$NZBUP_BRANCH" = "STABLE"; then
    VER_FIELD="stable-version"
    DNL_FIELD="stable-download"
    SIG_FIELD="stable-signature"
else
    echo "[ERROR] Unsupported branch $NZBUP_BRANCH"
    exit 1
fi

VER=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$VER_FIELD.*: \"\(.*\)\".*/\1/p"`
DNL_LINK=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$DNL_FIELD.*: \"\(.*\)\".*/\1/p"`
SIG_LINK=`cat "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt" | sed -n "s/^.*$SIG_FIELD.*: \"\(.*\)\".*/\1/p"`
rm -f "$NZBOP_TEMPDIR/NZBGET_UPDATE.txt"

SIGNATURE="nzbget-$VER.sig.txt"
echo "Downloading verification signature..."
rm -f "$NZBOP_TEMPDIR/$SIGNATURE"
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/$SIGNATURE" "$SIG_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

INSTALLER="nzbget-$VER-bin-linux.run"
echo "Downloading $INSTALLER..."
rm -f "$NZBOP_TEMPDIR/$INSTALLER"
"$NZBOP_APPBIN" -B webget "$NZBOP_TEMPDIR/$INSTALLER" "$DNL_LINK" 2>/dev/null
if test "$?" != "0"; then
    echo "[ERROR] Download failed, please try again later"
    exit 1
fi

echo "Verifying package authenticity..."
"$NZBOP_APPBIN" -B verify "$NZBOP_APPDIR/pubkey.pem" "$NZBOP_TEMPDIR/$SIGNATURE" "$NZBOP_TEMPDIR/$INSTALLER" 2>/dev/null
if test "$?" != "93"; then
    echo "[ERROR] Package authenticity verification failed"
    rm -f "$NZBOP_TEMPDIR/$INSTALLER"
    rm -f "$NZBOP_TEMPDIR/$SIGNATURE"
    exit 1
fi
rm -f "$NZBOP_TEMPDIR/$SIGNATURE"

echo "Updating NZBGet..."
echo "..."
sh "$NZBOP_TEMPDIR/$INSTALLER" --update --destdir "$NZBOP_APPDIR"
if test "$?" != "0"; then
    echo "[ERROR] Update failed, installer terminated with error status"
    exit 1
fi
rm -f "$NZBOP_TEMPDIR/$INSTALLER"
echo "..."
echo "Update completed"

# Recreating command line used to start NZBGet
CMDLINE=
N=1
while true
do
    PARAMNAME="NZBUP_CMDLINE$N"
    eval PARAM="\$NZBUP_CMDLINE${N}"
    if test "$PARAM" = ""; then
        break
    fi
    if test "$CMDLINE" != ""; then
        CMDLINE="$CMDLINE "
    fi
    CMDLINE="$CMDLINE\"$PARAM\""
    N=$((N + 1))
done

# If started in interactive console mode, inform user
if [ "$NZBUP_RUNMODE" != "DAEMON" ]; then
    echo "[WARNING] NZBGet was started in interactive console mode."
    echo "[WARNING] Automatic restart cannot re-attach to the terminal."
fi

echo "Restarting NZBGet..."
sleep 1
echo "[NZB] QUIT"

echo "Waiting for NZBGet to terminate"
while kill -0 "$NZBUP_PROCESSID" 2>/dev/null; do
    sleep 1
done

echo "Starting NZBGet..."

# Starting NZBGet
eval "$NZBOP_APPBIN" $CMDLINE

exit 0
