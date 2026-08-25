#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  DupeArticleFallback functional harness — Linux-in-Docker convenience runner.
#  Runs the scenarios in an ubuntu:24.04 container against a Linux nzbget
#  binary built for x86_64 (dynamically linked against the distro libs).
#
#  Usage: run-linux-docker.sh <path-to-linux-x86_64-nzbget-binary> [scenario]
#
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
NZBGET="${1:?usage: run-linux-docker.sh <linux-nzbget-binary> [scenario]}"
SCENARIO="${2:-all}"
[ -f "$NZBGET" ] || { echo "error: binary not found: $NZBGET" >&2; exit 2; }

exec docker run --rm --platform linux/amd64 \
	-v "$NZBGET":/usr/local/bin/nzbget:ro \
	-v "$HERE":/harness:ro \
	ubuntu:24.04 sh -c '
		export DEBIAN_FRONTEND=noninteractive
		apt-get -qq update >/dev/null 2>&1
		apt-get -qq install -y python3 python3-cryptography 7zip libxml2 libssl3t64 zlib1g libncursesw6 >/dev/null 2>&1
		python3 /harness/harness.py --nzbget /usr/local/bin/nzbget --target local --scenario '"$SCENARIO"'
	'
