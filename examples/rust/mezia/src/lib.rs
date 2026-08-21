use mezia_sys as ffi;
use std::ffi::{CStr, CString, NulError};
use std::os::raw::c_void;
use std::ptr;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum Error {
    #[error("failed to create session")]
    Create,
    #[error("invalid argument")]
    InvalidArg,
    #[error("session not ready")]
    NotReady,
    #[error("network error")]
    Network,
    #[error("SDP write failed")]
    SdpWrite,
    #[error("send audio failed")]
    SendAudio,
    #[error("send video failed")]
    SendVideo,
    #[error("playout failed")]
    Playout,
    #[error("string conversion error: {0}")]
    StringConversion(#[from] NulError),
    #[error("UTF-8 error: {0}")]
    Utf8(#[from] std::string::FromUtf8Error),
}

impl From<i32> for Error {
    fn from(code: i32) -> Self {
        match code {
            ffi::MEZON_ERR_INVALID_ARG => Error::InvalidArg,
            ffi::MEZON_ERR_NOT_READY => Error::NotReady,
            ffi::MEZON_ERR_NETWORK => Error::Network,
            _ => Error::InvalidArg,
        }
    }
}

pub struct Stats {
    pub packets_sent: u64,
    pub packets_received: u64,
    pub bytes_sent: u64,
    pub bytes_received: u64,
    pub audio_frames_encoded: u64,
    pub audio_frames_decoded: u64,
}

impl From<ffi::mezon_stats_t> for Stats {
    fn from(raw: ffi::mezon_stats_t) -> Self {
        Stats {
            packets_sent: raw.packets_sent,
            packets_received: raw.packets_received,
            bytes_sent: raw.bytes_sent,
            bytes_received: raw.bytes_received,
            audio_frames_encoded: raw.audio_frames_encoded,
            audio_frames_decoded: raw.audio_frames_decoded,
        }
    }
}

pub struct SessionConfig {
    pub local_ip: String,
    pub local_port: u16,
    pub remote_ip: String,
    pub remote_port: u16,
    pub mtu: usize,
    pub offer_audio: bool,
    pub offer_video: bool,
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self {
            local_ip: "127.0.0.1".into(),
            local_port: 0,
            remote_ip: "127.0.0.1".into(),
            remote_port: 9000,
            mtu: ffi::MEZON_DEFAULT_MTU,
            offer_audio: true,
            offer_video: false,
        }
    }
}

pub struct Session {
    ptr: *mut ffi::mezia_session_t,
    _local_ip: CString,
    _remote_ip: CString,
}

unsafe impl Send for Session {}

impl Session {
    pub fn new(config: SessionConfig) -> Result<Self, Error> {
        let local_ip = CString::new(config.local_ip)?;
        let remote_ip = CString::new(config.remote_ip)?;

        let ffi_config = ffi::mezia_session_config_t {
            local_ip: local_ip.as_ptr(),
            local_port: config.local_port,
            remote_ip: remote_ip.as_ptr(),
            remote_port: config.remote_port,
            mtu: config.mtu,
            offer_audio: config.offer_audio as i32,
            offer_video: config.offer_video as i32,
            ice_ufrag: ptr::null(),
            ice_pwd: ptr::null(),
            fingerprint: ptr::null(),
            on_audio: None,
            on_nal: None,
            user_data: ptr::null_mut(),
        };

        let ptr = unsafe { ffi::mezia_session_create(&ffi_config) };
        if ptr.is_null() {
            return Err(Error::Create);
        }

        Ok(Session {
            ptr,
            _local_ip: local_ip,
            _remote_ip: remote_ip,
        })
    }

    pub fn create_offer(&mut self) -> Result<String, Error> {
        let mut buf = vec![0u8; 8192];
        let mut len = 0;

        let status = unsafe {
            ffi::mezia_session_create_offer(self.ptr, buf.as_mut_ptr(), buf.len(), &mut len)
        };

        if status != ffi::MEZON_OK {
            return Err(Error::SdpWrite);
        }

        buf.truncate(len);
        Ok(String::from_utf8(buf)?)
    }

    pub fn set_local_description(&mut self, sdp: &str) -> Result<(), Error> {
        let sdp_c = CString::new(sdp)?;
        let status = unsafe { ffi::mezia_session_set_local_description(self.ptr, sdp_c.as_ptr()) };
        if status != ffi::MEZON_OK {
            return Err(status.into());
        }
        Ok(())
    }

    pub fn set_remote_description(&mut self, sdp: &str) -> Result<(), Error> {
        let sdp_c = CString::new(sdp)?;
        let status =
            unsafe { ffi::mezia_session_set_remote_description(self.ptr, sdp_c.as_ptr()) };
        if status != ffi::MEZON_OK {
            return Err(status.into());
        }
        Ok(())
    }

    pub fn start(&mut self) -> Result<(), Error> {
        let status = unsafe { ffi::mezia_session_start(self.ptr) };
        if status != ffi::MEZON_OK {
            return Err(status.into());
        }
        Ok(())
    }

    pub fn send_audio(&mut self, pcm: &[i16]) -> Result<(), Error> {
        if pcm.len() != ffi::MEZON_OPUS_FRAME_SAMPLES {
            return Err(Error::InvalidArg);
        }
        let status =
            unsafe { ffi::mezia_session_send_audio(self.ptr, pcm.as_ptr(), pcm.len()) };
        if status != ffi::MEZON_OK {
            return Err(Error::SendAudio);
        }
        Ok(())
    }

    pub fn playout_audio(&mut self) -> Result<(), Error> {
        let now_ns = unsafe { ffi::mezon_clock_now_ns() };
        let status = unsafe { ffi::mezia_session_playout_audio(self.ptr, now_ns) };
        if status != ffi::MEZON_OK && status != ffi::MEZON_ERR_NOT_READY {
            return Err(Error::Playout);
        }
        Ok(())
    }

    pub fn get_stats(&self) -> Stats {
        let mut raw = unsafe { std::mem::zeroed() };
        unsafe { ffi::mezia_session_get_stats(self.ptr, &mut raw) };
        raw.into()
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        unsafe { ffi::mezia_session_close(self.ptr) }
    }
}

pub fn clock_now_ns() -> u64 {
    unsafe { ffi::mezon_clock_now_ns() }
}

pub const OPUS_FRAME_SAMPLES: usize = ffi::MEZON_OPUS_FRAME_SAMPLES;
pub const OPUS_SAMPLE_RATE: u32 = ffi::MEZON_OPUS_SAMPLE_RATE;
