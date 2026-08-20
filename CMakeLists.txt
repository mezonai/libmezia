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
    src/audio/bitrate_controller.c

    src/video/video.c
    src/video/h264.c
    src/video/packetizer.c
    src/video/jitter.c

    src/transport/packet.c
    src/transport/audio_feedback.c

    src/sdp/sdp.c
    src/session/session.c
    src/rtcp/rtcp.c

    src/p2p/p2p.c
)

target_include_directories(mezia
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
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
        platform/ios/platform_ios.m
        platform/ios/audio.m
        platform/ios/camera.m
        platform/ios/video_encoder.m
        platform/ios/video_decoder.m
    )
    set_target_properties(mezia_ios PROPERTIES
        COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(mezia_ios
        PUBLIC mezia
        PRIVATE
            "-framework AVFoundation"
            "-framework VideoToolbox"
            "-framework CoreMedia"
            "-framework CoreVideo"
            "-framework AudioToolbox"
    )
    target_include_directories(mezia_ios
        PUBLIC include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios
    )
endif()

if(MEZON_BUILD_ANDROID)
    if(ANDROID AND ANDROID_NATIVE_API_LEVEL AND ANDROID_NATIVE_API_LEVEL LESS 26)
        message(FATAL_ERROR "MEZON_BUILD_ANDROID requires Android API 26+ for AAudio")
    endif()
    add_library(mezia_android STATIC
        platform/android/platform_android.cpp
        platform/android/audio.cpp
        platform/android/camera.cpp
        platform/android/video_encoder.cpp
        platform/android/video_decoder.cpp
    )
    find_library(ANDROID_MEDIA_LIB mediandk)
    find_library(ANDROID_CAMERA_LIB camera2ndk)
    find_library(ANDROID_AAUDIO_LIB aaudio)
    find_library(ANDROID_LOG_LIB log)
    target_link_libraries(mezia_android
        PUBLIC mezia
        PRIVATE
            ${ANDROID_MEDIA_LIB}
            ${ANDROID_CAMERA_LIB}
            ${ANDROID_AAUDIO_LIB}
            ${ANDROID_LOG_LIB}
            android
    )
    target_include_directories(mezia_android
        PUBLIC include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
            ${CMAKE_CURRENT_SOURCE_DIR}/platform/android
    )
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

    add_executable(test_bitrate_controller tests/test_bitrate_controller.c)
    target_include_directories(test_bitrate_controller PRIVATE src/audio)
    target_link_libraries(test_bitrate_controller PRIVATE mezia)
    add_test(NAME bitrate_controller COMMAND test_bitrate_controller)

    add_executable(test_audio_feedback tests/test_audio_feedback.c)
    target_include_directories(test_audio_feedback PRIVATE src/transport)
    target_link_libraries(test_audio_feedback PRIVATE mezia)
    add_test(NAME audio_feedback COMMAND test_audio_feedback)

    add_executable(test_adaptive_audio_link tests/test_adaptive_audio_link.c)
    target_include_directories(test_adaptive_audio_link PRIVATE
        src/audio src/transport src/internal)
    target_link_libraries(test_adaptive_audio_link PRIVATE mezia)
    add_test(NAME adaptive_audio_link COMMAND test_adaptive_audio_link)

    add_executable(test_video tests/test_video.c)
    target_link_libraries(test_video PRIVATE mezia)
    add_test(NAME video COMMAND test_video)

    add_executable(test_udp_loopback tests/test_udp_loopback.c)
    target_link_libraries(test_udp_loopback PRIVATE mezia Threads::Threads)
    add_test(NAME udp_loopback COMMAND test_udp_loopback)

    add_executable(test_platform_helpers tests/test_platform_helpers.c)
    target_include_directories(test_platform_helpers PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform)
    target_link_libraries(test_platform_helpers PRIVATE mezia)
    add_test(NAME platform_helpers COMMAND test_platform_helpers)

    add_executable(test_sdp tests/test_sdp.c)
    target_link_libraries(test_sdp PRIVATE mezia)
    add_test(NAME sdp COMMAND test_sdp)

    add_executable(test_session tests/test_session.c)
    target_link_libraries(test_session PRIVATE mezia)
    add_test(NAME session COMMAND test_session)

    add_executable(test_rtcp tests/test_rtcp.c)
    target_link_libraries(test_rtcp PRIVATE mezia)
    add_test(NAME rtcp COMMAND test_rtcp)
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
