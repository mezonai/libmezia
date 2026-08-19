#ifndef MEZIA_PLATFORM_ANDROID_INTERNAL_H
#define MEZIA_PLATFORM_ANDROID_INTERNAL_H

#include "mezia/platform.h"
#include "../common/audio_ring.h"

#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mezia_platform {
  mezia_platform_config_t config;
  mezia_ctx_t *media;
  mezon_audio_config_t audio_config;
  mezon_video_config_t video_config;
  int running;

  void *capture_stream;
  void *playback_stream;
  pthread_t playout_thread;
  int playout_running;
  mezia_audio_ring_t capture_ring;
  mezia_audio_ring_t playback_ring;
  int16_t capture_frame[MEZON_OPUS_FRAME_SAMPLES];
  int16_t playback_frame[MEZON_OPUS_FRAME_SAMPLES];
  pthread_mutex_t audio_lock;

  void *camera_manager;
  void *camera_device;
  void *capture_session;
  void *image_reader;
  char camera_id[64];
  mezia_camera_facing_t camera_facing;

  void *encoder;
  void *decoder;
  pthread_t encode_thread;
  pthread_t decode_thread;
  int encode_running;
  int decode_running;
  uint32_t video_bitrate_bps;
  uint8_t *sps;
  size_t sps_len;
  uint8_t *pps;
  size_t pps_len;
  uint8_t *yuv_scratch;
  size_t yuv_scratch_size;
  void *native_window;
};

void mezia_platform_report_error(mezia_platform_t *platform,
                                 mezon_status_t status, const char *where);

mezon_status_t mezia_android_audio_start(mezia_platform_t *platform);
void mezia_android_audio_stop(mezia_platform_t *platform);

mezon_status_t mezia_android_camera_start(mezia_platform_t *platform);
void mezia_android_camera_stop(mezia_platform_t *platform);
mezon_status_t mezia_android_camera_set_facing(mezia_platform_t *platform,
                                               mezia_camera_facing_t facing);

mezon_status_t mezia_android_encoder_start(mezia_platform_t *platform);
void mezia_android_encoder_stop(mezia_platform_t *platform);
mezon_status_t mezia_android_encoder_push_image(mezia_platform_t *platform,
                                                void *image);
mezon_status_t mezia_android_encoder_set_bitrate(mezia_platform_t *platform,
                                                 uint32_t bitrate_bps);

mezon_status_t mezia_android_decoder_start(mezia_platform_t *platform);
void mezia_android_decoder_stop(mezia_platform_t *platform);
void mezia_android_decoder_on_nal(const uint8_t *nal, size_t nal_len,
                                  uint32_t rtp_timestamp,
                                  int end_of_access_unit, int discontinuity,
                                  void *user_data);

#ifdef __cplusplus
}
#endif

#endif
