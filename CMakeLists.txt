cmake_minimum_required(VERSION 3.16)
project(mezon_mezia VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MEZON_BUILD_IOS "Build iOS platform bridge" OFF)
option(MEZON_BUILD_ANDROID "Build Android platform bridge" OFF)
option(MEZON_BUILD_EXAMPLES "Build examples" ON)

if(MEZON_BUILD_IOS)
    if(NOT APPLE)
        message(FATAL_ERROR "MEZON_BUILD_IOS requires building on macOS with Xcode")
    endif()
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

find_package(Threads REQUIRED)

if(NOT TARGET Opus::opus AND NOT TARGET opus)
    find_package(Opus CONFIG QUIET)
endif()

if(TARGET Opus::opus)
    set(MEZON_OPUS_TARGET Opus::opus)
elseif(TARGET opus)
    set(MEZON_OPUS_TARGET opus)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPUS QUIET IMPORTED_TARGET opus)
    endif()
    if(TARGET PkgConfig::OPUS)
        set(MEZON_OPUS_TARGET PkgConfig::OPUS)
    else()
        find_path(OPUS_INCLUDE_DIR NAMES opus/opus.h)
        find_library(OPUS_LIBRARY NAMES opus)
        if(NOT OPUS_INCLUDE_DIR OR NOT OPUS_LIBRARY)
            message(FATAL_ERROR
                "libopus is required. Install libopus-dev/opus-devel or provide OPUS_INCLUDE_DIR and OPUS_LIBRARY.")
        endif()
        add_library(mezon_opus UNKNOWN IMPORTED)
        set_target_properties(mezon_opus PROPERTIES
            IMPORTED_LOCATION "${OPUS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPUS_INCLUDE_DIR}")
        set(MEZON_OPUS_TARGET mezon_opus)
    endif()
endif()
message(STATUS "libmezia Opus target: ${MEZON_OPUS_TARGET}")

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

    src/transport/packet.c

    src/p2p/p2p.c
)

target_include_directories(mezia
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
)
target_link_libraries(mezia
    PUBLIC ${MEZON_OPUS_TARGET}
    PRIVATE Threads::Threads
)

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

include(CTest)
if(BUILD_TESTING)
    add_executable(test_packet tests/test_packet.c)
    target_include_directories(test_packet PRIVATE src/internal)
    target_link_libraries(test_packet PRIVATE mezia)
    add_test(NAME packet COMMAND test_packet)

    add_executable(test_audio tests/test_audio.c)
    target_link_libraries(test_audio PRIVATE mezia)
    add_test(NAME audio COMMAND test_audio)

    add_executable(test_video tests/test_video.c)
    target_link_libraries(test_video PRIVATE mezia)
    add_test(NAME video COMMAND test_video)

    add_executable(test_udp_loopback tests/test_udp_loopback.c)
    target_link_libraries(test_udp_loopback PRIVATE mezia Threads::Threads)
    add_test(NAME udp_loopback COMMAND test_udp_loopback)
endif()

if(MEZON_BUILD_EXAMPLES)
    add_executable(mezia_loopback examples/loopback.c)
    target_link_libraries(mezia_loopback PRIVATE mezia Threads::Threads)
endif()

install(TARGETS mezia
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
