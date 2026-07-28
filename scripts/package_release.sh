#!/bin/zsh
set -euo pipefail

PROJECT_ROOT=${0:A:h:h}
VERSION=$(<"$PROJECT_ROOT/VERSION")
BUILD_ROOT="$PROJECT_ROOT/.build"
DIST_ROOT="$PROJECT_ROOT/dist"
APP_ROOT="$DIST_ROOT/NoteMD.app"
CONTENTS="$APP_ROOT/Contents"
DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

cd "$PROJECT_ROOT"
DEVELOPER_DIR="$DEVELOPER_DIR" swift build -c release --product NoteMD

/bin/rm -rf "$APP_ROOT"
mkdir -p "$CONTENTS/MacOS" "$CONTENTS/Resources"
cp "$BUILD_ROOT/release/NoteMD" "$CONTENTS/MacOS/NoteMD"
cp "$PROJECT_ROOT/Packaging/Info.plist" "$CONTENTS/Info.plist"
cp "$PROJECT_ROOT/Assets/NoteMD-AppIcon-1024.png" "$CONTENTS/Resources/NoteMDIcon-v3.png"

codesign --force --deep --sign - "$APP_ROOT"

ditto -c -k --sequesterRsrc --keepParent \
    "$APP_ROOT" "$DIST_ROOT/NoteMD-$VERSION-macOS.zip"

pkgbuild \
    --component "$APP_ROOT" \
    --install-location /Applications \
    "$DIST_ROOT/NoteMD-$VERSION.pkg"

echo "$APP_ROOT"
echo "$DIST_ROOT/NoteMD-$VERSION-macOS.zip"
echo "$DIST_ROOT/NoteMD-$VERSION.pkg"
