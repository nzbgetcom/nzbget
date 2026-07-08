#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  DupeArticleFallback functional harness — Android runner (emulator/device).
#  Pushes an Android (aarch64) nzbget binary to a connected emulator/device
#  via adb and runs the scenarios there (harness --target adb). nserv and the
#  daemon run on the device; the RPC control port is forwarded to the host.
#
#  Prerequisites:
#    * adb on PATH, with exactly one emulator/device connected ("adb devices").
#      Start an emulator first, e.g.:
#        emulator -avd <name> -no-window -no-audio &
#        adb wait-for-device
#    * an Android (aarch64) nzbget build. Build with:
#        bash linux/build-nzbget.sh android aarch64-ndk release
#      then point this script at the resulting binary.
#
#  Usage: run-android.sh <path-to-android-nzbget-binary> [scenario] [adb-serial]
#
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
NZBGET_HOST="${1:?usage: run-android.sh <android-nzbget-binary> [scenario] [serial]}"
SCENARIO="${2:-all}"
SERIAL="${3:-}"

command -v adb >/dev/null 2>&1 || { echo "error: adb not found on PATH" >&2; exit 2; }
[ -f "$NZBGET_HOST" ] || { echo "error: binary not found: $NZBGET_HOST" >&2; exit 2; }

ADB="adb"
[ -n "$SERIAL" ] && ADB="adb -s $SERIAL"

# confirm a single device is reachable
DEVCOUNT=$($ADB devices | awk 'NR>1 && $2=="device"' | wc -l | tr -d ' ')
if [ "$DEVCOUNT" = "0" ]; then
	echo "error: no adb device/emulator connected (see 'adb devices')" >&2
	exit 2
fi

DEV_DIR=/data/local/tmp/nzbget-dupefallback-bin
DEV_BIN="$DEV_DIR/nzbget"
$ADB shell mkdir -p "$DEV_DIR"
$ADB push "$NZBGET_HOST" "$DEV_BIN" >/dev/null
$ADB shell chmod 755 "$DEV_BIN"
echo "pushed nzbget to $DEV_BIN"
$ADB shell "$DEV_BIN" -v 2>/dev/null | head -1 || true

exec python3 "$HERE/harness.py" --nzbget "$DEV_BIN" --target adb \
	--scenario "$SCENARIO" ${SERIAL:+--serial "$SERIAL"}
