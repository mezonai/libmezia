cmake_minimum_required(VERSION 3.16)
project(mezon_mezia C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MEZON_BUILD_IOS "Build iOS platform bridge" OFF)
option(MEZON_BUILD_ANDROID "Build Android platform bridge" OFF)

if(MEZON_BUILD_IOS)
    if(NOT APPLE)
        message(FATAL_ERROR "MEZON_BUILD_IOS requires building on macOS with Xcode")
    endif()
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

add_library(mezia STATIC
    src/media/media.c
    src/media/clock.c

    src/audio/audio.c
    src/audio/opus.c
    src/audio/jitter.c

    src/video/video.c
    src/video/h264.c
    src/video/packetizer.c
    src/video/jitter.c

    src/transport/udp.c
    src/transport/packet.c
    src/transport/transport.c
    src/transport/congestion.c

    src/p2p/p2p.c
    src/p2p/stun.c
    src/p2p/candidate.c
)

target_include_directories(mezia
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Opus QUIET)
if(Opus_FOUND)
    target_link_libraries(mezia PUBLIC Opus::opus)
endif()

if(WIN32)
    target_link_libraries(mezia PUBLIC ws2_32)
endif()

if(MEZON_BUILD_IOS)
    add_library(mezia_ios STATIC
        platform/ios/camera.m
        platform/ios/video_encoder.m
        platform/ios/audio.m
    )
    target_link_libraries(mezia_ios
        PUBLIC mezia
        PRIVATE
            "-framework AVFoundation"
            "-framework VideoToolbox"
            "-framework CoreMedia"
            "-framework CoreVideo"
    )
    target_include_directories(mezia_ios PUBLIC include)
endif()

if(MEZON_BUILD_ANDROID)
    add_library(mezia_android STATIC
        platform/android/camera.cpp
        platform/android/video_encoder.cpp
        platform/android/audio.cpp
    )
    find_library(ANDROID_MEDIA_LIB mediandk)
    find_library(ANDROID_CAMERA_LIB camera2ndk)
    find_library(ANDROID_LOG_LIB log)
    target_link_libraries(mezia_android
        PUBLIC mezia
        PRIVATE
            ${ANDROID_MEDIA_LIB}
            ${ANDROID_CAMERA_LIB}
            ${ANDROID_LOG_LIB}
    )
    target_include_directories(mezia_android PUBLIC include)
endif()

install(TARGETS mezia
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
