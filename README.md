# libmezia

`libmezia` is a small C11 media data-plane library for ultra-low-latency native
mobile applications. The portable core currently provides:

- Opus voice audio over RTP: 48 kHz mono, 20 ms frames, 24 kbit/s constrained
  VBR, DTX, in-band FEC, and packet-loss concealment;
- H.264 NAL packetization using RFC 6184 single-NAL and FU-A payloads; H.264
  encoding and decoding remain platform hardware responsibilities;
- direct, unencrypted UDP with a dedicated receive worker;
- a bounded 40–120 ms audio jitter/playout path designed for variable 4G delay.

Opus is intentionally the only software codec. Uncompressed PCM is not sent on
the network: 48 kHz mono PCM requires about 768 kbit/s before headers, while the
default Opus voice payload is approximately 24 kbit/s before RTP/UDP/IP
overhead.

## Security and network scope

Traffic is unencrypted and unauthenticated. Use it only on trusted networks or
inside another protected tunnel. Anyone able to reach the UDP port may observe
or inject media. STUN, ICE, TURN, SRTP, peer authentication, RTCP,
retransmission, pacing, and congestion control are not implemented yet.

Peers use application-provided direct IP addresses and UDP ports. The receiver
accepts packets only from the configured remote endpoint.

## Requirements

- CMake 3.16 or newer
- C11 and C++17 compilers
- libopus development headers and library
- platform thread/socket support

Install Opus on common desktop systems:

```bash
# Debian/Ubuntu
sudo apt install libopus-dev

# Fedora
sudo dnf install opus-devel

# macOS
brew install opus
```

CMake searches for an Opus package target, then pkg-config, then
`OPUS_INCLUDE_DIR` and `OPUS_LIBRARY`. Configuration fails if Opus is absent.
Android and iOS builds must provide a target-architecture libopus package; the
build must not link a host desktop library while cross-compiling.

## Build and test

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

## Audio contract

The first mobile profile is fixed so both peers agree without a signaling
layer:

| Setting | Value |
|---|---:|
| PCM application boundary | signed 16-bit, 48 kHz, mono |
| Frame duration | 20 ms / 960 samples |
| Opus application | VOIP |
| Bitrate | 24 kbit/s constrained VBR |
| Complexity | 5 |
| DTX / in-band FEC | enabled |
| Expected packet loss | 10% |
| RTP clock | 48 kHz |
| Default RTP payload type | 96 |
| Default jitter target / maximum | 60 ms / 120 ms |

`mezia_send_audio()` accepts exactly 960 PCM samples and emits one Opus RTP
packet synchronously. The audio capture thread should call it every 20 ms.

UDP receive callbacks only copy compressed payloads into fixed-capacity jitter
storage. The audio rendering thread calls `mezia_playout_audio()` every 20 ms.
Before startup buffering completes it returns `MEZON_ERR_NOT_READY`; afterwards
it produces one 960-sample callback per tick. A missing packet is recovered from
the following packet with FEC when available, otherwise Opus PLC is used.
Prolonged absence transitions to silence rather than allowing unbounded PLC or
latency.

Audio packetization, receive admission, and playout perform no steady-state
heap allocation. Packet payload buffers supplied to lower-level packetizers are
caller-owned and are never retained.

## Video contract

`mezon_video_ctx_t` accepts one H.264 NAL without an Annex-B start code and
emits/consumes RFC 6184 single-NAL or FU-A payloads. The caller marks the final
NAL of an access unit. The default dynamic payload type is 97. Software H.264
encoding or decoding is not included.

## Build options

| Option | Default | Description |
|---|---:|---|
| `BUILD_TESTING` | CMake default | Build RTP, Opus, H.264, and UDP tests |
| `MEZON_BUILD_EXAMPLES` | `ON` | Build the portable-core example |
| `MEZON_BUILD_IOS` | `OFF` | Build the currently skeletal iOS bridge |
| `MEZON_BUILD_ANDROID` | `OFF` | Build the currently skeletal Android bridge |

## Current limitations and next milestones

The portable core does not yet provide audio-device capture/playout, camera
capture, hardware H.264 sessions, A/V clock synchronization, keyframe requests,
RTCP feedback, congestion control, NAT traversal, signaling, or cryptographic
protection. The next platform milestone should connect AVFoundation and
VideoToolbox on iOS, and AAudio/Camera2/MediaCodec on Android, to the existing
PCM and H.264 boundaries.
