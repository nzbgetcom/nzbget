#!/bin/sh
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
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

set -e

# This script is used for signing and optionally notarizing nzbget mac os packages
# Script takes nzbget macos package from *-bin-macos-*.zip file, signs binaries in package and package itself.
# Then creates dmg with package, signs and optionally notarizes it

# pre-requisites
# 1. create-dmg: https://github.com/create-dmg/create-dmg
# 2. Apple Developer ID Application certificate. DEVELOPER_IDENTITY == Certificate User ID
# 3. Notarization: AppStore Connect API key with Developer role, placed in $HOME/.private_keys/. NOTARY_KEY_ID == Key ID, NOTARY_KEY_ISSUER == Key Issuer ID

# check command-line parameters
APP_ARCHIVE=$1
if [ -z $APP_ARCHIVE ]; then
    echo "No NZBGet archive provided. Exiting."
    exit 1
fi

# test needed vars
if [ -z "$KEYCHAIN_PASSWORD" ] && [ -z "$DEVELOPER_IDENTITY" ]; then
    echo "No config vars found. Exiting"
    exit 1
fi

if [ "$NOTARIZE" == "true" ]; then
    if [ -z "$NOTARY_KEY_ID" ] && [ -z "$NOTARY_KEY_ISSUER" ]; then
      echo "No config vars found for notarization. Exiting"
      exit 1
    fi
fi

DMG_NAME=${APP_ARCHIVE/-bin-macos/}
DMG_NAME=${DMG_NAME/.zip/.dmg}

# unzip archive
unzip "$APP_ARCHIVE"

SIGN_FILES="\
MacOS/NZBGet \
Resources/daemon/usr/local/bin/7za \
Resources/daemon/usr/local/bin/unrar \
Resources/daemon/usr/local/bin/nzbget"

# sign binary files in package
security unlock-keychain -p $KEYCHAIN_PASSWORD ~/Library/Keychains/login.keychain-db
for SIGN_FILE in $SIGN_FILES; do
    codesign -s "$DEVELOPER_IDENTITY" -f --timestamp -o runtime -i "com.nzbget" "NZBGet.app/Contents/$SIGN_FILE"
done

# sign package itself
codesign -s "$DEVELOPER_IDENTITY" -f --timestamp -o runtime -i "com.nzbget" "NZBGet.app"

# create dmg
create-dmg \
  --volname "NZBGet" \
  --window-pos 100 100 \
  --window-size 435 240 \
  --icon-size 80 \
  --icon "NZBGet.app" 100 95 \
  --hide-extension "NZBGet.app" \
  --app-drop-link 300 95 \
  --background backgound.png \
  "$DMG_NAME" \
  "NZBGet.app/"

# sign dmg and check
codesign -s "$DEVELOPER_IDENTITY" -f --timestamp -o runtime -i "com.nzbget" "$DMG_NAME"
codesign -vvv --deep --strict "$DMG_NAME"

# cleanup
rm -rf NZBGet.app

# notarize dmg - optional
# useful commands:
# notarization history:
# > xcrun notarytool history --key "$HOME/.private_keys/AuthKey_$NOTARY_KEY_ID.p8" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_KEY_ISSUER"
# notarization log:
# > xcrun notarytool log "$SUBMISSION_ID" --key "$HOME/.private_keys/AuthKey_$NOTARY_KEY_ID.p8" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_KEY_ISSUER" "$SUBMISSION_ID.log"
# notarization check:
# > spctl -a -vvv -t install "$DMG_NAME"
if [ "$NOTARIZE" == "true" ]; then
    xcrun notarytool submit "$DMG_NAME" --key "$HOME/.private_keys/AuthKey_$NOTARY_KEY_ID.p8" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_KEY_ISSUER" --wait
    xcrun stapler staple "$DMG_NAME"
fi
