# libmezia Rust Bindings

Safe Rust wrapper for `libmezia` via FFI.

## Structure

```
examples/rust/
├── mezia-sys/       Raw FFI bindings (unsafe)
├── mezia/           Safe Rust wrapper
└── simple-client/   Example client application
```

## Prerequisites

- Rust 1.70+ (install via [rustup](https://rustup.rs))
- libopus development headers
- Build tools (gcc, make, cmake)

```bash
# Ubuntu/Debian
sudo apt install libopus-dev build-essential

# macOS
brew install opus
```

## Build

From `examples/rust/`:

```bash
cargo build --release
```

This compiles the entire `libmezia` C codebase and links it statically into the Rust binary.

## Run

```bash
cargo run --bin simple-client
```

Output:

```
libmezia Rust client example
=============================

Creating session...
Creating SDP offer...

Generated SDP offer:
v=0
o=- 0 0 IN IP4 0.0.0.0
s=-
t=0 0
a=group:BUNDLE 0
a=ice-ufrag:mezia
...

Session started. Sending silent audio frames for 5 seconds...
Frame 0/250: sent 1 packets, 51 bytes
Frame 50/250: sent 51 packets, 2601 bytes
...
```

## Use in your project

Add to `Cargo.toml`:

```toml
[dependencies]
mezia = { path = "../path/to/libmezia/examples/rust/mezia" }
```

Example:

```rust
use mezia::{Session, SessionConfig, OPUS_FRAME_SAMPLES};

let config = SessionConfig {
    local_ip: "0.0.0.0".into(),
    remote_ip: "192.168.1.5".into(),
    remote_port: 9000,
    offer_audio: true,
    ..Default::default()
};

let mut session = Session::new(config)?;
let offer = session.create_offer()?;
// Send offer to signaling server
session.set_local_description(&offer)?;
session.set_remote_description(&answer)?;
session.start()?;

// Audio loop (20ms frames, 960 samples @ 48kHz)
let mut pcm = vec![0i16; OPUS_FRAME_SAMPLES];
loop {
    // Fill pcm from microphone
    session.send_audio(&pcm)?;
    std::thread::sleep(Duration::from_millis(20));
}
```

## Safety

The `mezia` crate wraps unsafe FFI calls in safe Rust APIs:
- RAII: `Session` auto-closes on `Drop`
- Ownership: no double-free, session is `!Clone`
- Type safety: slices instead of raw pointers
- Error handling: `Result<T, Error>` instead of error codes

The raw `mezia-sys` crate is `unsafe` and matches the C ABI exactly.

## Platform

Works on Linux, macOS, and Windows (with MSVC or MinGW). The `cc` crate handles cross-platform C compilation.
