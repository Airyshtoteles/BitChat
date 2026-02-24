# Android NDK Build Configuration for BitChat JNI
#
# Integrated with Gradle via app/build.gradle.kts externalNativeBuild.

cmake_minimum_required(VERSION 3.22)
project(bitchat_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Root of the BitChat source tree.
set(BITCHAT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../)

# ---------- Android log ----------
find_library(log-lib log)

# ---------- libsodium (prebuilt static library) ----------
add_library(sodium STATIC IMPORTED)

set(JNILIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs)

set_target_properties(sodium PROPERTIES
    IMPORTED_LOCATION ${JNILIBS_DIR}/${ANDROID_ABI}/libsodium.a
)

# libsodium headers (shared across ABIs)
set(SODIUM_INCLUDE_DIR ${JNILIBS_DIR}/include)

# ---------- sqlite3 (compiled from amalgamation) ----------
set(SQLITE3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3)

add_library(sqlite3 STATIC ${SQLITE3_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${SQLITE3_DIR})
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    HAVE_USLEEP=1
    HAVE_LOCALTIME_R=1
)
# Suppress warnings in third-party code
target_compile_options(sqlite3 PRIVATE -w)

# ---------- BitChat core sources ----------
file(GLOB_RECURSE BITCHAT_SOURCES
    ${BITCHAT_ROOT}/src/domain/*.c
    ${BITCHAT_ROOT}/src/usecase/*.c
    ${BITCHAT_ROOT}/src/infra/*.c
    ${BITCHAT_ROOT}/src/bridge/bitchat_ffi.c
    ${BITCHAT_ROOT}/src/bridge/bitchat_jni.c
)

add_library(bitchat_jni SHARED ${BITCHAT_SOURCES})

target_include_directories(bitchat_jni
    PRIVATE
        ${BITCHAT_ROOT}/include
        ${SODIUM_INCLUDE_DIR}
        ${SQLITE3_DIR}
)

target_compile_definitions(bitchat_jni PRIVATE BITCHAT_BUILD_JNI)

target_compile_options(bitchat_jni PRIVATE
    -Wall -Wextra -Wpedantic
)

# Link against libsodium, sqlite3, log
target_link_libraries(bitchat_jni
    sodium
    sqlite3
    ${log-lib}
)
