use mezia::{Session, SessionConfig, OPUS_FRAME_SAMPLES};
use std::thread;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libmezia Rust client example");
    println!("=============================\n");

    let config = SessionConfig {
        local_ip: "127.0.0.1".into(),
        local_port: 0,
        remote_ip: "127.0.0.1".into(),
        remote_port: 9000,
        mtu: 1200,
        offer_audio: true,
        offer_video: false,
    };

    println!("Creating session...");
    let mut session = Session::new(config)?;

    println!("Creating SDP offer...");
    let offer = session.create_offer()?;
    println!("\nGenerated SDP offer:");
    println!("{}", offer);
    println!();

    println!("Setting local description...");
    session.set_local_description(&offer)?;

    println!("Setting remote description (loopback)...");
    session.set_remote_description(&offer)?;

    println!("Starting session...");
    session.start()?;

    println!("\nSession started. Sending silent audio frames for 5 seconds...");
    println!("(In a real app, read from microphone)\n");

    let mut pcm = vec![0i16; OPUS_FRAME_SAMPLES];
    let frame_duration = Duration::from_millis(20);
    let frames_to_send = 5 * 1000 / 20; // 5 seconds

    for frame_num in 0..frames_to_send {
        // Generate a simple sine wave instead of silence for demo
        for i in 0..OPUS_FRAME_SAMPLES {
            let t = (frame_num * OPUS_FRAME_SAMPLES + i) as f32;
            let freq = 440.0; // A4 note
            let sample = (t * freq * 2.0 * std::f32::consts::PI / 48000.0).sin();
            pcm[i] = (sample * 8000.0) as i16;
        }

        match session.send_audio(&pcm) {
            Ok(_) => {
                if frame_num % 50 == 0 {
                    let stats = session.get_stats();
                    println!(
                        "Frame {}/{}: sent {} packets, {} bytes",
                        frame_num, frames_to_send, stats.packets_sent, stats.bytes_sent
                    );
                }
            }
            Err(e) => eprintln!("Failed to send audio frame {}: {}", frame_num, e),
        }

        thread::sleep(frame_duration);
    }

    let stats = session.get_stats();
    println!("\nFinal stats:");
    println!("  Packets sent: {}", stats.packets_sent);
    println!("  Bytes sent: {}", stats.bytes_sent);
    println!("  Audio frames encoded: {}", stats.audio_frames_encoded);
    println!("\nSession closing...");

    Ok(())
}
