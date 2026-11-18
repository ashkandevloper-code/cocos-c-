# NEXORA — cocos2d-x v4.0 (C++17) client. Layout follows the cocos2d-x v4 cpp template.
#  * Android: proj.android (Gradle externalNativeBuild -> this file, target "nexora").
#  * Linux desktop (optional): cmake -S . -B build-linux && cmake --build build-linux
#  * Host unit tests (no engine needed): see tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.6)

set(APP_NAME nexora)
project(${APP_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED BUILD_ENGINE_DONE)
    if(DEFINED ENV{COCOS2DX_ROOT} AND NOT DEFINED COCOS2DX_ROOT_PATH)
        set(COCOS2DX_ROOT_PATH $ENV{COCOS2DX_ROOT})
    endif()
    if(NOT DEFINED COCOS2DX_ROOT_PATH)
        set(COCOS2DX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cocos2d)
    endif()
    if(NOT EXISTS ${COCOS2DX_ROOT_PATH}/cocos/cocos2d.h)
        message(FATAL_ERROR "cocos2d-x 4.0 not found at ${COCOS2DX_ROOT_PATH}. Run tools/fetch_cocos.sh (or set COCOS2DX_ROOT).")
    endif()
    set(CMAKE_MODULE_PATH ${COCOS2DX_ROOT_PATH}/cmake/Modules/)
    include(CocosBuildSet)
    add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos ${ENGINE_BINARY_PATH}/cocos/core)
endif()

# cocos2d-x 4.0's CMake forces C++11 on its targets; our code needs C++17.
set(CMAKE_CXX_STANDARD 17)

set(NX_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include(${NX_ROOT}/cmake/nx_core_sources.cmake)
nx_generate_config(${CMAKE_CURRENT_BINARY_DIR}/generated)

set(GAME_RES_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
if(APPLE OR WINDOWS)
    cocos_mark_multi_resources(common_res_files RES_TO "Resources" FOLDERS ${GAME_RES_FOLDER})
endif()

file(GLOB_RECURSE NX_APP_SOURCES CONFIGURE_DEPENDS
    ${NX_ROOT}/Classes/AppDelegate.cpp
    ${NX_ROOT}/Classes/app/*.cpp
    ${NX_ROOT}/Classes/platform/*.cpp
    ${NX_ROOT}/Classes/ui/*.cpp
    ${NX_ROOT}/Classes/scenes/*.cpp
)
file(GLOB_RECURSE NX_APP_HEADERS CONFIGURE_DEPENDS
    ${NX_ROOT}/Classes/*.h
)
set(GAME_SOURCE ${NX_CORE_SOURCES} ${NX_APP_SOURCES})
set(GAME_HEADER ${NX_APP_HEADERS})

if(ANDROID)
    list(APPEND GAME_SOURCE proj.android/app/jni/hellocpp/main.cpp proj.android/app/jni/nx_jni.cpp)
elseif(LINUX)
    list(APPEND GAME_SOURCE proj.linux/main.cpp)
elseif(WINDOWS)
    list(APPEND GAME_SOURCE proj.win32/main.cpp ${common_res_files})
endif()

set(all_code_files ${GAME_HEADER} ${GAME_SOURCE})
if(NOT ANDROID)
    add_executable(${APP_NAME} ${all_code_files})
else()
    add_library(${APP_NAME} SHARED ${all_code_files})
    add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos/platform/android ${ENGINE_BINARY_PATH}/cocos/platform)
    target_link_libraries(${APP_NAME} -Wl,--whole-archive cpp_android_spec -Wl,--no-whole-archive)
    # cocos2d-x 4.0 lists libcurl.a after libssl/libcrypto; GNU ld/gold then miss OpenSSL symbols.
    # lld (shipped with NDK r21) resolves static archives order-independently.
    set_property(TARGET ${APP_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -fuse-ld=lld")
endif()

target_link_libraries(${APP_NAME} cocos2d)
target_include_directories(${APP_NAME}
    PRIVATE Classes
    PRIVATE third_party
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    PRIVATE ${COCOS2DX_ROOT_PATH}/cocos/audio/include/
)
set_target_properties(${APP_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Deterministic game physics: forbid FMA contraction (clang on arm64 fuses a*b+c by default), so the
# fixed-step simulations stay bit-identical to the Dart VM and between phones.
target_compile_options(${APP_NAME} PRIVATE -ffp-contract=off)

setup_cocos_app_config(${APP_NAME})
if(LINUX OR WINDOWS)
    cocos_get_resource_path(APP_RES_DIR ${APP_NAME})
    cocos_copy_target_res(${APP_NAME} LINK_TO ${APP_RES_DIR} FOLDERS ${GAME_RES_FOLDER})
endif()
