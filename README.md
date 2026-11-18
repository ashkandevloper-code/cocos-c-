# NEXORA — cocos2d-x 4.0 (C++17) client

Port of the NEXORA Flutter app (Riverpod / go_router / Supabase / LiveKit) to cocos2d-x v4.
The backend is unchanged: the same two Supabase projects (main = auth/social, games = rooms/matches/tournaments),
same tables, RPCs and Edge Functions (`supabase/` is copied verbatim from the Flutter repo).

| Path | What |
|---|---|
| `Classes/core` | Engine-independent C++: JSON, Dart-exact RNG, Persian shaper/bidi, settings, registries |
| `Classes/games` | Game rules / bots / physics (deterministic, golden-tested against the Dart code) |
| `Classes/backend` | Supabase over HTTP (GoTrue, PostgREST, RPC, Functions, Storage), Realtime (Phoenix), MatchLink |
| `Classes/platform` | cocos2d-x glue: libcurl HTTP, WebSocket, UserDefault, JNI bridge |
| `Classes/ui`, `Classes/scenes` | cocos2d-x scenes and widgets |
| `tests/` | Host-only unit tests (`cmake -S tests -B build-host && cmake --build build-host && build-host/nx_tests`) |
| `proj.android` | Gradle project (AGP 4.1.3, Gradle 6.7.1, NDK r21e, minSdk 24, arm64-v8a + armeabi-v7a) |
| `docs/PORTING.md` | Dart module → C++ file map with status and known gaps |

Build locally: `tools/fetch_cocos.sh` (clones cocos2d-x 4.0 into `./cocos2d` + third-party libs),
`python3 tools/gen_config.py` (with `SUPABASE_URL` etc. in the environment), then
`cd proj.android && ./gradlew assembleRelease`.
