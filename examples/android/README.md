# Android demo

Minimal two-phone tester for the AAudio / Camera2 / MediaCodec bridge.

UDP is **unencrypted**. Use a trusted LAN only.

## Open in Android Studio

1. File → Open → `examples/android`
2. Install NDK + CMake from SDK Manager (NDK r26+, CMake 3.22.1)
3. First native configure fetches [libopus 1.5.2](https://github.com/xiph/opus) via CMake `FetchContent` (needs network)
4. Run on a physical device (API 26+). Emulators are a poor AAudio/camera test.

ABI is `arm64-v8a` only.

## Two-device call

1. Both phones on the same Wi-Fi
2. Grant microphone + camera
3. Phone A: remote IP = Phone B, local UDP `5004`, remote UDP `5004`
4. Phone B: remote IP = Phone A, same ports (or swap if you bind different ports)
5. Tap **Start** on both

The black `SurfaceView` is the **remote** decoder output. Local camera is captured but not previewed.

Uncheck **Send/receive H.264** for an audio-only path.

## Notes

- SSRCs are random per start; that is fine because each `mezia` instance only cares about its own send SSRC and accepts the peer’s packets by UDP endpoint, not SSRC.
- Video bitrate is fixed at 1.5 Mbit/s in this demo. Cap it in a real app with `mezia_platform_set_video_bitrate`.
- There is still no keyframe request: a lost IDR freezes the picture until the next one (~2 s GOP).
