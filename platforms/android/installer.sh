#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2015-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
#  Copyright (C) 2024 phnzb <pavel@nzbget.com>
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

# Strict error handling for debugging
set -o nounset
set -o errexit

# Installer title
TITLE=
# Size of installer script (package header)
HEADER=
# Size of installer package (header + payload)
TOTAL=
# Md5 sum of payload
MD5=
# List of included CPU architecture binaries
DISTARCHS=
# Target platform
PLATFORM=

SCRIPTFILE=$(cd "$(dirname "$0")" 2>/dev/null && pwd)/$(basename "$0")
[ -f "$SCRIPTFILE" ] || SCRIPTFILE="$0"

SILENT=no
ALLARCHS="$DISTARCHS"
ARCH=""
SELECT=auto
OUTDIR="nzbget"
PRINTEDTITLE=no
JUSTUNPACK=no
UPDATE=no
VERIFY=yes
OS=""

Info()
{
    if test "$SILENT" = "no"; then
        echo "$1"
    fi
}

Error()
{
    Info "ERROR: $1"
    exit 1
}

ValidArch()
{
    for A in $ALLARCHS
    do
        if test "$1" = "$A"; then
            return 0
        fi
    done
    return 1
}

ResolveArch()
{
    _REQ=$1

    # Direct match in package
    if ValidArch "$_REQ"; then
        echo "$_REQ"
        return 0
    fi

    # Architecture alias resolution based on architectures bundled in ALLARCHS
    case $_REQ in
        aarch64|arm64)
            if ValidArch "arm64"; then echo "arm64"; return 0; fi
            if ValidArch "aarch64"; then echo "aarch64"; return 0; fi
            ;;
        armv7*|armv8*|armhf|armv7)
            if ValidArch "armv7"; then echo "armv7"; return 0; fi
            if ValidArch "armhf"; then echo "armhf"; return 0; fi
            ;;
        i386|i686|x86)
            if ValidArch "x86"; then echo "x86"; return 0; fi
            if ValidArch "i686"; then echo "i686"; return 0; fi
            ;;
        x86_64|amd64)
            if ValidArch "x86_64"; then echo "x86_64"; return 0; fi
            if ValidArch "amd64"; then echo "amd64"; return 0; fi
            ;;
    esac

    echo "$_REQ"
    return 1
}

PrintArch()
{
    if ValidArch $1 || ValidArch $(ResolveArch $1); then
        Info "$2"
    fi
}

PrintHelp()
{
    # Check if command 'basename' is available and fallback to full path if it's not
    TEST=`eval TEST='$(basename /base/bin)' 2>/dev/null; echo $TEST`
    if test "$TEST" != ""; then
        EXENAME=$(basename $0)
    else
        EXENAME=$0
    fi

    Info "Usage: sh $EXENAME [OPTIONS]"
    Info ""
    Info "Options:"
    Info "  --help              Print this help screen"
    Info "  --info              Print package info"
    Info "  --version           Print program version"
    Info "  --arch ARCH         Target CPU architecture"
    Info "  --destdir DIR       Target directory (default: \"$OUTDIR\")"
    Info "  --silent            Do not print status messages"
    Info "  --unpack            Unpack files into target directory without configuring"
    Info "  --no-verify         Do not verify package integrity before unpacking"
    Info ""
    Info "For more options and details see documentation."
    Info ""
    if test "$PRINTEDTITLE" = "no"; then
        Info "Package info for $TITLE:"
        Info ""
    fi
    Info "This installer supports Android API 21 or newer and the following CPU architectures:"
    PrintArch "arm64"    "    arm64    - ARMv8/AArch64, 64 Bit"
    PrintArch "armv7"    "    armv7    - ARMv7, 32 Bit"
    PrintArch "x86_64"   "    x86_64   - x86, 64 Bit"
    PrintArch "x86"      "    x86      - x86, 32 Bit"
    Info ""

    Info "Default target CPU architecture: $ARCH"
    Info ""
    Info "To install nzbget using default options run:"
    Info "  sh $EXENAME"
    Info ""
    Info "To install nzbget for specific CPU architecture into \"/path/to/nzbget\" run:"
    Info "  sh $EXENAME --arch $ARCH --destdir /path/to/nzbget"
    Info ""
    Info "To unpack all files into current directory without configuring run:"
    Info "  sh $EXENAME --unpack --destdir ."
    Info ""
    Info "For documentation and support please visit https://nzbget.com"
}

Verify()
{
    CALCHEADER=`eval CALCHEADER='$(wc -c "$SCRIPTFILE" 2>/dev/null)' 2>/dev/null; echo $CALCHEADER`
    if test "$CALCHEADER" = ""; then
        # Command "wc -c" failed, skipping verification
        return 0
    fi

    # Evaluating file size, wc produces something like "12345 filename", parsing it
    CALCTOTAL=0
    for SIZE in $CALCHEADER
    do
        CALCTOTAL=$SIZE
        break
    done

    if test "$CALCTOTAL" != "$TOTAL"; then
        Error "Package is corrupt: size check failed (calculated: $CALCTOTAL, expected: $TOTAL)"
    fi

    CALCMD5=`dd "if=$SCRIPTFILE" bs=$HEADER skip=1 2>/dev/null | md5sum 2>/dev/null | cut -b-32 2>/dev/null | cat`
    if test "$CALCMD5" != ""; then
        # Evaluating md5 sum, md5sum produces something like "12345 -", parsing it
        for DIGEST in $CALCMD5
        do
            CALCMD5=$DIGEST
            break
        done
        if test "$CALCMD5" != "$MD5"; then
            Error "Package is corrupt: MD5 check failed (calculated: $CALCMD5, expected: $MD5)"
        fi
    fi
}

DetectArch()
{
    OS=`uname -s`
    if test "$OS" != "Linux"; then
        PrintHelp
        Error "Operating system ($OS) isn't supported by this installer."
    fi

    if test "$UPDATE" = "yes"; then
        ARCH=`cat "$OUTDIR/installer.cfg" 2>/dev/null | sed -n 's/^arch=\(.*\)$/\1/p'`
        SELECT=`cat "$OUTDIR/installer.cfg" 2>/dev/null | sed -n 's/^select=\(.*\)$/\1/p'`
    fi

    if test "$ARCH" = ""; then
        CPU=`uname -m`
        case $CPU in
            i386|i686|x86)
                ARCH=$(ResolveArch "x86")
                ;;
            x86_64|amd64)
                ARCH=$(ResolveArch "x86_64")
                ;;
            armv7*|armv8*|armhf|armv7)
                ARCH=$(ResolveArch "armv7")
                ;;
            aarch64|arm64)
                ARCH=$(ResolveArch "arm64")
                ;;
            *)
                ARCH=$(ResolveArch "$CPU")
                ;;
        esac
    fi

    if ! ValidArch $ARCH; then
        PrintHelp
        Error "Could not detect target CPU architecture ($CPU)"
    fi
}

Unpack()
{
    mkdir -p "$OUTDIR" || Error "Could not create target directory ($OUTDIR)"
    (
        cd "$OUTDIR" || exit 1
        dd "if=$SCRIPTFILE" bs=$HEADER skip=1 2>/dev/null | gzip -c -d | tar xf - || exit 1

        if test "$JUSTUNPACK" = "no"; then
            for A in $ALLARCHS
            do
                if test "$A" != "$ARCH"; then
                    rm -f nzbget-$A
                    rm -f unrar-$A
                    rm -f unrar7-$A
                    rm -f 7za-$A
                fi
            done
            rm -f nzbget
            rm -f unrar
            rm -f unrar7
            rm -f 7za
            if test -f "nzbget-$ARCH"; then
                mv "nzbget-$ARCH" nzbget
            else
                Error "NZBGet binary for architecture ($ARCH) is missing from this package."
            fi
            test -f "unrar-$ARCH" && mv "unrar-$ARCH" unrar || true
            test -f "unrar7-$ARCH" && mv "unrar7-$ARCH" unrar7 || true
            test -f "7za-$ARCH" && mv "7za-$ARCH" 7za || true
            echo "arch=$ARCH" > "installer.cfg"
            echo "select=$SELECT" >> "installer.cfg"
        fi
    ) || Error "Unpacking failed."
}

Configure()
{
    cd "$OUTDIR"
    QUICKHELP=no

    if test ! -f nzbget.conf; then
        cp ./webui/nzbget.conf.template nzbget.conf
        QUICKHELP=yes
    fi
}

# ParseCommandLine
while true
do
    case "${1:-}" in
        -h|--help)
            PrintHelp
            exit 0
            ;;
        --info)
            Info "Package info for $TITLE:"
            Info ""
            Info "Supported CPU architectures:"
            PrintArch "arm64"    "    arm64    - ARMv8/AArch64, 64 Bit"
            PrintArch "armv7"    "    armv7    - ARMv7, 32 Bit"
            PrintArch "x86_64"   "    x86_64   - x86, 64 Bit"
            PrintArch "x86"      "    x86      - x86, 32 Bit"
            Info ""
            if test "$ARCH" != ""; then
                Info "Default target CPU architecture: $ARCH"
            fi
            exit 0
            ;;
        --version)
            Info "$TITLE"
            exit 0
            ;;
        --arch)
            ARCH=${2:-}
            SELECT=manual
            if test "$ARCH" = "all"; then
                PrintHelp
                Info ""
                Error "Target CPU architecture cannot be 'all'. Use --unpack to unpack all architectures without configuring."
            fi
            ARCH=$(ResolveArch "$ARCH")
            if ! ValidArch $ARCH; then
                PrintHelp
                Info ""
                Error "Unsupported target CPU architecture ($ARCH)"
            fi
            shift 2
            ;;
        --destdir)
            OUTDIR=${2:-}
            shift 2
            ;;
        --silent)
            SILENT=yes
            shift
            ;;
        --unpack)
            JUSTUNPACK=yes
            shift
            ;;
        --update)
            UPDATE=yes
            shift
            ;;
        --no-verify)
            VERIFY=no
            shift
            ;;
        "")
            break
            ;;
        *)
            PrintHelp
            Info ""
            Error "Invalid option $1"
            ;;
    esac
done

Info "Installer for $TITLE"
if test "$SILENT" = "no"; then
    PRINTEDTITLE=yes
fi

if test "$VERIFY" = "yes"; then
    Info "Verifying package..."
    Verify
fi

if test "$JUSTUNPACK" = "no"; then
    Info "Checking system..."
    DetectArch
    if test "$SELECT" = "manual"; then
        Info "CPU-Architecture: $ARCH (manually set)"
    else
        Info "CPU-Architecture: $ARCH"
    fi
fi

Info "Unpacking..."
Unpack

ABSOUTDIR=`cd "$OUTDIR"; pwd`

if test "$JUSTUNPACK" = "no"; then
    if test ! -f "$OUTDIR/nzbget"; then
        Error "Installation failed: executable ($OUTDIR/nzbget) was not created."
    fi
    Info "Configuring..."
    Configure

    Info "Installation completed"

    if test "$QUICKHELP" = "yes" -a "$SILENT" = "no"; then
        Info ""
        Info "Quick help (from nzbget-directory):"
        Info "   ./nzbget -s        - start nzbget in console mode"
        Info "   ./nzbget -D        - start nzbget in daemon mode (in background)"
        Info "   ./nzbget -C        - connect to background process"
        Info "   ./nzbget -Q        - stop background process"
        Info "   ./nzbget -h        - help screen with all commands"
        Info ""
        Info "Successfully installed into $ABSOUTDIR"

        # Trying to get current IP-address        
        IP=""
        INTERFACE=""
        {
            # First find default interface (ie 'wlan0') with the 'route' command
            INTERFACE=`route -n | sed -rn 's/^0.0.0.0.* ([^ ]+)$/\1/p'`
        } > /dev/null 2>&1
        if test "$INTERFACE" != ""; then
            # OK, a default route 0.0.0.0 + corresponding interface was found
            # Now find the IPv4 address on that interface:
            {
                IP=`ifconfig "$INTERFACE" | sed -rn 's/.*r:([^ ]+) .*/\1/p'`
            } > /dev/null 2>&1
        fi
        if test "$IP" = ""; then
            IP="localhost"
        fi

        Info "Web-interface is on http://$IP:6789 (login:nzbget, password:tegbzn6789)"
    else
        Info "Successfully installed into $ABSOUTDIR"
    fi
    Info "For support please visit https://nzbget.com/forum/"
else
    Info "Unpacked into $ABSOUTDIR"
fi

exit
#END-OF-INSTALLER
