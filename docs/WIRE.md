# Wire contract with mezon-sfu

Compatibility is **protocol**, not API. `libmezia` stays a direct
Opus / H.264 / RTP / RTCP / (later SRTP / DTLS / ICE) pipeline.

## What mezon-sfu actually uses today

Inspected in `/mnt/mezonai/mezon-sfu` (2026-08-19):

| Item | SFU today |
|---|---|
| Audio | Opus PT **111**, `minptime=10;useinbandfec=1` |
| Video | **VP9 / AV1 / VP8 + RTX**. No H.264 in `sfu_video_codec_t` |
| Transport | BUNDLE, `rtcp-mux`, ICE trickle, DTLS-SRTP `setup` |
| RTP extensions | TWCC id **6**, MID id **7** |
| RTCP | NACK, PLI, TWCC feedback, SR/RR on the path |
| MIDs | local audio `0`, camera `1`, screen `2` |

So a mobile 1-to-1 **audio** path can match the SFU now. **Video through
the current SFU is not H.264.** Hardware H.264 remains the right *client*
encode; the SFU must grow H.264 (or libmezia must packetize VP8) before
1-to-1 video interops.

## libmezia 8-component cap (1-to-1)

Keep: Opus, H.264 RTP (RFC 6184), RTP, SRTP, DTLS, ICE/STUN, tiny RTCP,
jitter buffer.

Do not add: Track / Transceiver / Sender / Receiver / simulcast / SVC /
SFU subscription / PeerConnection.

RTCP in this tree is only SR, RR, NACK, PLI. TWCC later if measurements
need it.

ICE: host + srflx + STUN + check + nomination, via a mature library, not
a from-scratch ICE stack. TURN stays a service-layer config.

## Pay the stack trade-off incrementally

2026 goal: smallest engine that beats WebRTC on mobile 1-to-1
(latency, voice, CPU/battery) **through the existing SFU**. Browsers
keep WebRTC. Native clients use libmezia. Meetings come after 1-to-1
is proven.
