#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  DupeArticleFallback functional harness — local runner (Linux, macOS).
#  Runs the scenarios against a locally-built nzbget binary.
#
#  Usage: run-local.sh <path-to-nzbget-binary> [scenario]
#
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
NZBGET="${1:?usage: run-local.sh <nzbget-binary> [scenario]}"
SCENARIO="${2:-all}"

if [ ! -x "$NZBGET" ]; then
	echo "error: nzbget binary not found/executable: $NZBGET" >&2
	exit 2
fi

exec python3 "$HERE/harness.py" --nzbget "$NZBGET" --target local --scenario "$SCENARIO"
