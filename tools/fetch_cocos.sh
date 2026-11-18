#!/usr/bin/env bash
# Fetches cocos2d-x 4.0 into ./cocos2d (or $1) with its prebuilt third-party libs, non-interactively.
set -euo pipefail
DEST="${1:-$(cd "$(dirname "$0")/.." && pwd)/cocos2d}"
TAG="cocos2d-x-4.0"
if [ -f "$DEST/cocos/cocos2d.h" ] && [ -f "$DEST/external/config.json" ] && [ -d "$DEST/external/freetype2" ]; then
  echo "cocos2d-x already present at $DEST"; exit 0
fi
echo "::group::clone $TAG"
rm -rf "$DEST"
git clone --depth 1 --branch "$TAG" https://github.com/cocos2d/cocos2d-x.git "$DEST"
echo "::endgroup::"
# download-deps.py asks interactive questions; do the same thing directly (version from external/config.json).
VER=$(python3 -c "import json;print(json.load(open('$DEST/external/config.json'))['version'])")
echo "::group::third-party libs $VER"
TMP=$(mktemp -d)
curl -fsSL --retry 5 -o "$TMP/ext.zip" "https://github.com/cocos2d/cocos2d-x-3rd-party-libs-bin/archive/$VER.zip"
python3 - "$TMP/ext.zip" "$TMP/x" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY
cp -R "$TMP/x/cocos2d-x-3rd-party-libs-bin-$VER/." "$DEST/external/"
rm -rf "$TMP"
echo "::endgroup::"
# Keep the zip-free tree small: drop platforms we never build.
rm -rf "$DEST"/external/*/prebuilt/{ios,mac,tvos,win32,win10,tizen} 2>/dev/null || true
rm -rf "$DEST/tests" "$DEST/templates/lua-template-default" "$DEST/templates/js-template-default" 2>/dev/null || true
# rapidjson bundled with cocos2d-x 4.0 has an ill-formed GenericStringRef::operator= that GCC >= 14 rejects.
sed -i 's/GenericStringRef& operator=(const GenericStringRef& rhs) { s = rhs.s; length = rhs.length; }/GenericStringRef\& operator=(const GenericStringRef\& rhs) = delete;/' "$DEST/external/json/document.h" || true
echo "cocos2d-x ready at $DEST"
