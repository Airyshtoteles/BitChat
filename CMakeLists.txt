cmake_minimum_required(VERSION 3.14)
project(BitChat VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# Core library
file(GLOB_RECURSE BITCHAT_SOURCES
    src/domain/*.c
    src/usecase/*.c
    src/infra/*.c
    src/bridge/bitchat_ffi.c
)

add_library(bitchat STATIC ${BITCHAT_SOURCES})

target_include_directories(bitchat
    PUBLIC  ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${SODIUM_INCLUDE_DIRS}
            ${SQLITE3_INCLUDE_DIRS}
)

target_link_libraries(bitchat
    PUBLIC  ${SODIUM_LIBRARIES}
            ${SQLITE3_LIBRARIES}
            pthread
)

target_compile_options(bitchat
    PRIVATE ${SODIUM_CFLAGS_OTHER}
            ${SQLITE3_CFLAGS_OTHER}
)

# JNI bridge (optional, only when building for Android)
option(BITCHAT_BUILD_JNI "Build JNI bridge for Android" OFF)
if(BITCHAT_BUILD_JNI)
    find_package(JNI REQUIRED)
    add_library(bitchat_jni SHARED src/bridge/bitchat_jni.c)
    target_include_directories(bitchat_jni PRIVATE ${JNI_INCLUDE_DIRS})
    target_compile_definitions(bitchat_jni PRIVATE BITCHAT_BUILD_JNI)
    target_link_libraries(bitchat_jni PRIVATE bitchat)
endif()

# Tests
option(BITCHAT_BUILD_TESTS "Build unit tests" ON)
if(BITCHAT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# CLI simulator (localhost UDP mesh)
option(BITCHAT_BUILD_CLI "Build CLI simulator" ON)
if(BITCHAT_BUILD_CLI)
    add_executable(bitchat_cli src/cli/main.c)
    target_link_libraries(bitchat_cli PRIVATE bitchat)
endif()
