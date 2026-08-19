#ifndef MEZIA_PLATFORM_IOS_INTERNAL_H
#define MEZIA_PLATFORM_IOS_INTERNAL_H

#include "mezia/platform.h"
#include "../common/audio_ring.h"

#include <pthread.h>

#ifdef __OBJC__
#import <AVFoundation/AVFoundation.h>
#import <VideoToolbox/VideoToolbox.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct mezia_platform {
  mezia_platform_config_t config;
  mezia_ctx_t *media;
  mezon_audio_config_t audio_config;
  mezon_video_config_t video_config;
  int running;

#ifdef __OBJC__
  AVAudioEngine *engine;
  AVAudioConverter *converter;
  dispatch_queue_t audio_send_queue;
  dispatch_queue_t audio_playout_queue;
  dispatch_source_t playout_timer;
#else
  void *engine;
  void *converter;
  void *audio_send_queue;
  void *audio_playout_queue;
  void *playout_timer;
#endif
  mezia_audio_ring_t capture_ring;
  mezia_audio_ring_t playback_ring;
  int16_t capture_frame[MEZON_OPUS_FRAME_SAMPLES];
  int16_t playback_frame[MEZON_OPUS_FRAME_SAMPLES];
  pthread_mutex_t audio_lock;

#ifdef __OBJC__
  AVCaptureSession *capture;
  AVCaptureDeviceInput *camera_input;
  AVCaptureVideoDataOutput *video_output;
  id camera_delegate;
  dispatch_queue_t camera_queue;
#else
  void *capture;
  void *camera_input;
  void *video_output;
  void *camera_queue;
#endif
  mezia_camera_facing_t camera_facing;

#ifdef __OBJC__
  VTCompressionSessionRef compression;
  dispatch_queue_t encode_queue;
#else
  void *compression;
  void *encode_queue;
#endif
  uint32_t video_bitrate_bps;
  int sent_parameter_sets;

#ifdef __OBJC__
  VTDecompressionSessionRef decompression;
  CMVideoFormatDescriptionRef format_desc;
  dispatch_queue_t decode_queue;
#else
  void *decompression;
  void *format_desc;
  void *decode_queue;
#endif
  uint8_t *sps;
  size_t sps_len;
  uint8_t *pps;
  size_t pps_len;
  uint8_t *decode_annexb;
  size_t decode_annexb_cap;
};

void mezia_platform_report_error(mezia_platform_t *platform,
                                 mezon_status_t status, const char *where);

mezon_status_t mezia_ios_audio_start(mezia_platform_t *platform);
void mezia_ios_audio_stop(mezia_platform_t *platform);

mezon_status_t mezia_ios_camera_start(mezia_platform_t *platform);
void mezia_ios_camera_stop(mezia_platform_t *platform);
mezon_status_t mezia_ios_camera_set_facing(mezia_platform_t *platform,
                                           mezia_camera_facing_t facing);

mezon_status_t mezia_ios_encoder_start(mezia_platform_t *platform);
void mezia_ios_encoder_stop(mezia_platform_t *platform);
mezon_status_t mezia_ios_encoder_encode(mezia_platform_t *platform,
                                        void *sample_buffer);
mezon_status_t mezia_ios_encoder_set_bitrate(mezia_platform_t *platform,
                                             uint32_t bitrate_bps);

mezon_status_t mezia_ios_decoder_start(mezia_platform_t *platform);
void mezia_ios_decoder_stop(mezia_platform_t *platform);
void mezia_ios_decoder_on_nal(const uint8_t *nal, size_t nal_len,
                              uint32_t rtp_timestamp, int end_of_access_unit,
                              int discontinuity, void *user_data);

#ifdef __cplusplus
}
#endif

#endif
