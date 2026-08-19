cmake_minimum_required(VERSION 3.22)
project(mezia_android_app LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(MEZIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../..")

include(FetchContent)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    opus
    GIT_REPOSITORY https://github.com/xiph/opus.git
    GIT_TAG v1.5.2
)
FetchContent_MakeAvailable(opus)

set(MEZON_BUILD_ANDROID ON CACHE BOOL "" FORCE)
set(MEZON_BUILD_IOS OFF CACHE BOOL "" FORCE)
set(MEZON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MEZIA_ROOT}" "${CMAKE_BINARY_DIR}/libmezia")

add_library(mezia_jni SHARED "${MEZIA_ROOT}/platform/android/jni_bridge.cpp")
target_include_directories(mezia_jni PRIVATE
    "${MEZIA_ROOT}/include"
    "${MEZIA_ROOT}/src/internal"
    "${MEZIA_ROOT}/platform"
    "${MEZIA_ROOT}/platform/android"
)
target_link_libraries(mezia_jni
    PRIVATE
        mezia_android
        android
        log
)
