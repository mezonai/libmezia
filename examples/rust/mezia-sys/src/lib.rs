#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::os::raw::{c_char, c_int, c_void};

pub const MEZON_RTP_HEADER_SIZE: usize = 12;
pub const MEZON_DEFAULT_MTU: usize = 1200;
pub const MEZON_OPUS_FRAME_SAMPLES: usize = 960;
pub const MEZON_OPUS_SAMPLE_RATE: u32 = 48000;

pub const MEZON_OK: c_int = 0;
pub const MEZON_ERR_INVALID_ARG: c_int = -1;
pub const MEZON_ERR_NOMEM: c_int = -2;
pub const MEZON_ERR_INTERNAL: c_int = -3;
pub const MEZON_ERR_NOT_READY: c_int = -4;
pub const MEZON_ERR_BUFFER_TOO_SMALL: c_int = -5;
pub const MEZON_ERR_MALFORMED_PACKET: c_int = -6;
pub const MEZON_ERR_NETWORK: c_int = -7;

#[repr(C)]
pub struct mezia_session_t {
    _private: [u8; 0],
}

pub type mezon_audio_callback_t = Option<
    unsafe extern "C" fn(
        pcm: *const i16,
        samples_per_channel: usize,
        rtp_timestamp: u32,
        discontinuity: c_int,
        user_data: *mut c_void,
    ),
>;

pub type mezon_video_callback_t = Option<
    unsafe extern "C" fn(
        nal: *const u8,
        nal_len: usize,
        rtp_timestamp: u32,
        end_of_access_unit: c_int,
        discontinuity: c_int,
        user_data: *mut c_void,
    ),
>;

#[repr(C)]
pub struct mezia_session_config_t {
    pub local_ip: *const c_char,
    pub local_port: u16,
    pub remote_ip: *const c_char,
    pub remote_port: u16,
    pub mtu: usize,
    pub offer_audio: c_int,
    pub offer_video: c_int,
    pub ice_ufrag: *const c_char,
    pub ice_pwd: *const c_char,
    pub fingerprint: *const c_char,
    pub on_audio: mezon_audio_callback_t,
    pub on_nal: mezon_video_callback_t,
    pub user_data: *mut c_void,
}

#[repr(C)]
pub struct mezon_stats_t {
    pub packets_sent: u64,
    pub packets_received: u64,
    pub bytes_sent: u64,
    pub bytes_received: u64,
    pub malformed_packets: u64,
    pub unknown_payloads: u64,
    pub sequence_gaps: u64,
    pub duplicate_packets: u64,
    pub late_packets: u64,
    pub reassembly_failures: u64,
    pub socket_errors: u64,
    pub audio_frames_encoded: u64,
    pub audio_frames_decoded: u64,
    pub audio_frames_fec_recovered: u64,
    pub audio_frames_plc: u64,
    pub audio_frames_dropped: u64,
    pub audio_jitter_resets: u64,
    pub audio_jitter_underruns: u64,
    pub audio_adaptation_reports_sent: u64,
    pub audio_adaptation_reports_received: u64,
    pub audio_adaptation_reports_rejected: u64,
    pub audio_adaptation_stale_events: u64,
    pub audio_bitrate_increases: u64,
    pub audio_bitrate_decreases: u64,
    pub audio_current_bitrate_bps: u32,
    pub audio_current_packet_loss_percent: u8,
}

extern "C" {
    pub fn mezia_session_create(config: *const mezia_session_config_t) -> *mut mezia_session_t;
    pub fn mezia_session_close(session: *mut mezia_session_t);
    pub fn mezia_session_create_offer(
        session: *mut mezia_session_t,
        out: *mut u8,
        out_capacity: usize,
        out_len: *mut usize,
    ) -> c_int;
    pub fn mezia_session_set_local_description(
        session: *mut mezia_session_t,
        sdp: *const c_char,
    ) -> c_int;
    pub fn mezia_session_set_remote_description(
        session: *mut mezia_session_t,
        sdp: *const c_char,
    ) -> c_int;
    pub fn mezia_session_start(session: *mut mezia_session_t) -> c_int;
    pub fn mezia_session_send_audio(
        session: *mut mezia_session_t,
        pcm: *const i16,
        samples_per_channel: usize,
    ) -> c_int;
    pub fn mezia_session_playout_audio(session: *mut mezia_session_t, now_ns: u64) -> c_int;
    pub fn mezia_session_send_video(
        session: *mut mezia_session_t,
        nal: *const u8,
        nal_len: usize,
        rtp_timestamp: u32,
        end_of_access_unit: c_int,
    ) -> c_int;
    pub fn mezia_session_get_stats(session: *const mezia_session_t, stats: *mut mezon_stats_t);
    pub fn mezon_clock_now_ns() -> u64;
}
