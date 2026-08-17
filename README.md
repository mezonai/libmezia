# libmezon-media

P2P media transport library for Mezon's native mobile clients. Handles Opus
audio codec, H.264 RTP packetization/depacketization, UDP transport, and P2P
(STUN/ICE) — no software video codec. H.264 encode/decode is done by the
platform's hardware codec (VideoToolbox on iOS, MediaCodec on Android); this
library only packetizes and depacketizes already-encoded NAL units.

```
                 libmezon-media
                       │
          ┌────────────┴────────────┐
          │                         │
        Audio                     Video
          │                         │
        Opus                     H.264 (packetize only)
          │                         │
          └────────────┬────────────┘
                       │
                P2P UDP Transport
                       │
              ┌────────┴────────┐
              │                 │
            iOS               Android
```

## Prerequisites

| Target        | Requirements                                                    |
|---------------|-------------------------------------------------------------------|
| Core library  | CMake ≥ 3.16, a C11/C++17 compiler, [libopus](https://opus-codec.org/) |
| iOS bridge    | macOS + Xcode, CMake, `MEZON_BUILD_IOS=ON`                       |
| Android bridge| Android NDK (r21+), CMake, `MEZON_BUILD_ANDROID=ON`               |

### Installing Opus

```bash
# macOS
brew install opus

# Debian/Ubuntu
sudo apt install libopus-dev

# Or build from source: https://opus-codec.org/downloads/
```

If `find_package(Opus)` can't locate it, point CMake at it explicitly:

```bash
cmake -B build -DOpus_ROOT=/path/to/opus/install
```

## Build: core library only

Builds `libmezia.a` — the platform-independent audio/video/transport/p2p
code. No Xcode or NDK needed.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Output: `build/libmezia.a`
Headers: `include/mezia/*.h`

Install to a prefix (optional):

```bash
cmake --install build --prefix /path/to/install
```

## Build: iOS

Requires macOS + Xcode command line tools. Produces `libmezia_ios.a`,
which links `mezia` plus the VideoToolbox/AVFoundation bridge.

### Using the Xcode generator (device + simulator via `xcodebuild`)

```bash
cmake -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DMEZON_BUILD_IOS=ON

cmake --build build-ios --config Release
```

### Using a standalone toolchain (Makefiles, single architecture)

```bash
cmake -B build-ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DMEZON_BUILD_IOS=ON

cmake --build build-ios -j
```

For simulator builds on Apple Silicon, add
`-DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64`.

Output: `build-ios/libmezia_ios.a` (Release config under `Release-iphoneos/`
if using the Xcode generator)

Link the resulting static lib into your Xcode app target, along with the
`AVFoundation`, `VideoToolbox`, `CoreMedia`, and `CoreVideo` frameworks (already
attached automatically if you consume this via CMake `target_link_libraries`).

## Build: Android

Requires the Android NDK. Point `CMAKE_TOOLCHAIN_FILE` at the NDK's CMake
toolchain file (bundled with the NDK under `build/cmake/android.toolchain.cmake`).

```bash
export ANDROID_NDK=/path/to/Android/sdk/ndk/<version>

cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DMEZON_BUILD_ANDROID=ON

cmake --build build-android -j
```

Repeat with `-DANDROID_ABI=armeabi-v7a`, `x86`, or `x86_64` as needed for other
ABIs, using separate build directories per ABI.

Output: `build-android/libmezia_android.a`

This links `mezia` plus the Camera2/MediaCodec bridge (`libmediandk`,
`libcamera2ndk`, `liblog`). Consume the static lib from your app's own
`CMakeLists.txt` / Gradle `externalNativeBuild`, or bundle it into a `.so` via
your app module.

## CMake build options

| Option                | Default | Description                                    |
|------------------------|---------|-------------------------------------------------|
| `MEZON_BUILD_IOS`      | `OFF`   | Build the iOS platform bridge (`mezia_ios`)      |
| `MEZON_BUILD_ANDROID`  | `OFF`   | Build the Android platform bridge (`mezia_android`) |

## Project layout

```
libmezia/
├── CMakeLists.txt
├── include/mezia/     public headers
├── src/
│   ├── media/                lifecycle, clock
│   ├── audio/                Opus encode/decode, jitter buffer
│   ├── video/                H.264 NAL packetize/depacketize, jitter buffer
│   ├── transport/            UDP, RTP/RTCP packet handling, congestion control
│   └── p2p/                  STUN, ICE candidates, peer connection
└── platform/
    ├── ios/                  Camera + VideoToolbox + AVAudioEngine bridge
    └── android/               Camera2 + MediaCodec + AAudio bridge
```

## Consuming from another CMake project

```cmake
add_subdirectory(libmezia)
target_link_libraries(your_app PRIVATE mezia)
# or mezia_ios / mezia_android if built
```
