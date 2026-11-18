# Engine-independent sources shared by the app (CMakeLists.txt) and the host test build (tests/CMakeLists.txt).
file(GLOB_RECURSE NX_CORE_SOURCES CONFIGURE_DEPENDS
  ${NX_ROOT}/Classes/core/*.cpp
  ${NX_ROOT}/Classes/games/*.cpp
  ${NX_ROOT}/Classes/backend/*.cpp
)
file(GLOB_RECURSE NX_CORE_HEADERS CONFIGURE_DEPENDS
  ${NX_ROOT}/Classes/core/*.h
  ${NX_ROOT}/Classes/games/*.h
  ${NX_ROOT}/Classes/backend/*.h
)

# Generates NxConfig.generated.h from the environment (CI injects SUPABASE_URL, SUPABASE_ANON_KEY,
# GAMES_SUPABASE_URL, GAMES_SUPABASE_ANON_KEY, LIVEKIT_URL). An already generated header in
# Classes/config/ (written by tools/gen_config.py) wins, so Gradle builds that do not forward env still work.
function(nx_generate_config OUT_DIR)
  set(PRE ${NX_ROOT}/Classes/config/NxConfig.generated.h)
  file(MAKE_DIRECTORY ${OUT_DIR})
  if(EXISTS ${PRE})
    configure_file(${PRE} ${OUT_DIR}/NxConfig.generated.h COPYONLY)
    message(STATUS "NEXORA config: using Classes/config/NxConfig.generated.h")
    return()
  endif()
  set(NX_SUPABASE_URL "$ENV{SUPABASE_URL}")
  set(NX_SUPABASE_ANON_KEY "$ENV{SUPABASE_ANON_KEY}")
  set(NX_GAMES_SUPABASE_URL "$ENV{GAMES_SUPABASE_URL}")
  set(NX_GAMES_SUPABASE_ANON_KEY "$ENV{GAMES_SUPABASE_ANON_KEY}")
  set(NX_LIVEKIT_URL "$ENV{LIVEKIT_URL}")
  if(NX_LIVEKIT_URL STREQUAL "")
    set(NX_LIVEKIT_URL "wss://nexora-space-xzjzlfyl.livekit.cloud")
  endif()
  configure_file(${NX_ROOT}/Classes/config/NxConfig.generated.h.in ${OUT_DIR}/NxConfig.generated.h @ONLY)
  if(NX_SUPABASE_URL STREQUAL "")
    message(STATUS "NEXORA config: SUPABASE_URL not set -> offline build")
  else()
    message(STATUS "NEXORA config: online build for ${NX_SUPABASE_URL}")
  endif()
endfunction()
