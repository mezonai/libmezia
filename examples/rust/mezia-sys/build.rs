use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let libmezia_root = PathBuf::from(&manifest_dir)
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .to_path_buf();

    let src = libmezia_root.join("src");
    let include = libmezia_root.join("include");

    println!("cargo:warning=libmezia_root: {}", libmezia_root.display());
    println!("cargo:warning=src: {}", src.display());

    cc::Build::new()
        .file(src.join("media/media.c"))
        .file(src.join("media/clock.c"))
        .file(src.join("audio/audio.c"))
        .file(src.join("audio/opus.c"))
        .file(src.join("audio/jitter.c"))
        .file(src.join("audio/bitrate_controller.c"))
        .file(src.join("video/video.c"))
        .file(src.join("video/h264.c"))
        .file(src.join("video/packetizer.c"))
        .file(src.join("video/jitter.c"))
        .file(src.join("transport/packet.c"))
        .file(src.join("transport/audio_feedback.c"))
        .file(src.join("sdp/sdp.c"))
        .file(src.join("session/session.c"))
        .file(src.join("rtcp/rtcp.c"))
        .file(src.join("p2p/p2p.c"))
        .include(&include)
        .include(src.join("internal"))
        .include(src.join("audio"))
        .include(src.join("transport"))
        .warnings(false)
        .compile("mezia");

    println!("cargo:rustc-link-lib=opus");
    println!("cargo:rustc-link-lib=pthread");

    println!("cargo:rerun-if-changed={}", src.display());
    println!("cargo:rerun-if-changed={}", include.display());
}
