# Android NDK Build Configuration for BitChat JNI
#
# Usage (from your Android project's app/build.gradle.kts):
#
#   android {
#       externalNativeBuild {
#           cmake {
#               path = file("src/main/cpp/CMakeLists.txt")
#           }
#       }
#   }
#
# Or standalone NDK build:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#         -DANDROID_ABI=arm64-v8a \
#         -DBITCHAT_BUILD_JNI=ON \
#         -DBITCHAT_BUILD_TESTS=OFF \
#         ..
#   make

cmake_minimum_required(VERSION 3.22)
project(bitchat_android C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Root of the BitChat source tree (adjust if this file is copied elsewhere).
set(BITCHAT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ---------- libsodium (prebuilt or find_package) ----------
# Option A: Prebuilt .so shipped alongside the project.
#   set(SODIUM_LIB ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/${ANDROID_ABI}/libsodium.so)
#   add_library(sodium SHARED IMPORTED)
#   set_target_properties(sodium PROPERTIES IMPORTED_LOCATION ${SODIUM_LIB})
#
# Option B: Use pkg-config (if NDK sysroot has it).
find_library(log-lib log)

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
    PRIVATE ${BITCHAT_ROOT}/include
)

target_compile_definitions(bitchat_jni PRIVATE BITCHAT_BUILD_JNI)

target_compile_options(bitchat_jni PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

# Link against libsodium, sqlite3, log, and pthread.
# Adjust library names/paths to match your NDK setup.
target_link_libraries(bitchat_jni
    sodium
    sqlite3
    ${log-lib}
)
