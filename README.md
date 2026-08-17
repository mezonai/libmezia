# libmezia

`libmezia` is a small C11 media data-plane library for ultra-low-latency native
applications. The current portable-core milestone sends media over direct,
unencrypted UDP using a deliberately small RTP v2 subset:

- signed 16-bit interleaved PCM audio, carried as big-endian L16 payloads;
- H.264 NAL units produced by a platform hardware encoder, packetized as RFC
  6184 single-NAL or FU-A payloads;
- synchronous UDP sending and a dedicated receive callback thread;
- bounded packet/NAL sizes and freshness-first loss handling.

The library performs **no software encoding or decoding**. Android MediaCodec
and iOS VideoToolbox integration will supply H.264 in a later milestone. Audio
is uncompressed PCM, so bandwidth is `sample_rate * channels * 16` bits/s
before RTP/UDP/IP overhead (48 kHz stereo is about 1.536 Mbit/s).

## Security and network scope

Traffic is unencrypted and unauthenticated. Use this milestone only on trusted
networks or inside another protected tunnel. Anyone able to reach the UDP port
may observe or inject media. STUN, ICE, TURN, SRTP, peer authentication, RTCP,
retransmission, pacing, and congestion control are not implemented yet.

Peers use application-provided direct IP addresses and UDP ports. The receiver
accepts packets only from the configured remote endpoint.

## Build and test

Requirements: CMake 3.16+, a C11 compiler, and platform thread/socket support.
There is no Opus dependency.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/mezia_loopback
```

Strict development build:

```bash
cmake -S . -B build-strict \
  -DBUILD_TESTING=ON \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build-strict --parallel
ctest --test-dir build-strict --output-on-failure
```

## API model

- `mezon_peer_t` owns a UDP socket and one receive worker. `mezon_peer_send()`
  is synchronous; receive callbacks run on the worker thread.
- `mezon_audio_ctx_t` converts caller-owned host-endian PCM to/from RTP L16
  payloads. `samples_per_channel` is explicit.
- `mezon_video_ctx_t` accepts one H.264 NAL without an Annex-B start code and
  emits/consumes single-NAL and FU-A payloads. The caller marks the last NAL of
  an access unit.
- `mezia_ctx_t` combines one peer with optional audio and video contexts.
- Packet payload buffers are caller-owned. Packetizers never retain them and
  report required packet counts/capacities with `MEZON_ERR_BUFFER_TOO_SMALL`.
- Receive payload pointers are valid only for the duration of the callback.

Default dynamic RTP payload types are 96 for PCM and 97 for H.264. The default
UDP MTU is 1200 bytes.

## Build options

| Option | Default | Description |
|---|---:|---|
| `BUILD_TESTING` | CMake default | Build deterministic unit and UDP loopback tests |
| `MEZON_BUILD_EXAMPLES` | `ON` | Build the portable-core example |
| `MEZON_BUILD_IOS` | `OFF` | Build the currently skeletal iOS bridge |
| `MEZON_BUILD_ANDROID` | `OFF` | Build the currently skeletal Android bridge |

## Current limitations and next milestones

The portable core does not yet provide adaptive jitter buffering, audio device
capture/playout, camera capture, hardware encoder/decoder sessions, A/V clock
synchronization, keyframe requests, RTCP feedback, congestion control, NAT
traversal, or cryptographic protection. The next platform milestone should
connect AVFoundation/VideoToolbox and Camera2/MediaCodec/AAudio to the existing
PCM and H.264 boundaries without adding software codecs.
