#!/usr/bin/env bash
# Compiles every engine-dependent source (scenes, UI, platform) with -fsyntax-only against the cocos2d-x 4.0
# headers (Linux target). Catches cocos API misuse in seconds without building the engine.
# Usage: tools/syntax_check.sh [cocos_root]   (needs libglew-dev libglfw3-dev libcurl4-openssl-dev)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
C="${1:-${COCOS2DX_ROOT:-$ROOT/cocos2d}}"
GEN="$ROOT/build-syntax/generated"
mkdir -p "$GEN"
[ -f "$GEN/NxConfig.generated.h" ] || cat > "$GEN/NxConfig.generated.h" <<'H'
#pragma once
#define NX_CFG_SUPABASE_URL ""
#define NX_CFG_SUPABASE_ANON_KEY ""
#define NX_CFG_GAMES_SUPABASE_URL ""
#define NX_CFG_GAMES_SUPABASE_ANON_KEY ""
#define NX_CFG_LIVEKIT_URL ""
H
INC=(-I"$ROOT/Classes" -I"$ROOT/third_party" -I"$GEN" -I"$C" -I"$C/cocos" -I"$C/cocos/platform" -I"$C/cocos/editor-support"
     -I"$C/cocos/audio/include" -I"$C/extensions" -I"$C/external" -I"$C/external/glfw3/include/linux"
     -I"$C/external/freetype2/include/linux/freetype2" -I"$C/external/webp/include/linux" -I"$C/external/websockets/include/linux"
     -I"$C/external/openssl/include/linux" -I"$C/external/uv/include" -I"$C/external/xxhash" -I"$C/external/ConvertUTF"
     -I"$C/external/curl/include/linux" -I"$C/external/chipmunk/include" -I"$C/external/Box2D/include" -I"$C/external/png/include/linux"
     -I"$C/external/zlib/include/linux" -I"$C/external/jpeg/include/linux" -I"$C/external/clipper" -I"$C/external/edtaa3func"
     -I"$C/external/poly2tri" -I"$C/external/recast" -I"$C/external/tinyxml2" -I"$C/external/unzip" -I"$C/external/xxtea")
FAIL=0
N=0
while IFS= read -r f; do
  N=$((N+1))
  if ! g++ -std=c++17 -fsyntax-only -DLINUX -DCC_ENABLE_CHIPMUNK_INTEGRATION=1 -w "${INC[@]}" "$f" 2> /tmp/nx_syntax.log; then
    echo "::error file=$f::syntax check failed"; cat /tmp/nx_syntax.log | head -60; FAIL=1
  else
    echo "ok  $f"
  fi
done < <(find "$ROOT/Classes" -name '*.cpp' | sort)
echo "checked $N files"
exit $FAIL
