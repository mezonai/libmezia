#ifndef MEZIA_PLATFORM_H
#define MEZIA_PLATFORM_H

#include "mezia/media.h"
#include "mezia/peer.h"
#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Thin device-bridge around mezia_ctx_t. Audio RTP uses a 48 kHz clock
 * (+960 per 20 ms frame). Video RTP timestamps use a 90 kHz clock:
 *   ts90 = mezon_clock_now_ns() * 90000 / 1000000000
 *
 * Apps must request microphone/camera permission before start.
 * Video encoder bitrate is independent of Opus adaptation; cap it here
 * when the path is shared with H.264.
 */
typedef struct mezia_platform mezia_platform_t;

typedef enum {
  MEZIA_CAMERA_FRONT = 0,
  MEZIA_CAMERA_BACK = 1,
} mezia_camera_facing_t;

typedef void (*mezia_platform_error_callback_t)(mezon_status_t status,
                                                const char *where,
                                                void *user_data);
typedef void (*mezia_platform_frame_callback_t)(const void *native_image,
                                                uint32_t rtp_timestamp,
                                                void *user_data);

typedef struct {
  mezon_peer_config_t peer;
  uint32_t audio_ssrc;
  uint32_t video_ssrc;
  uint8_t audio_payload_type;
  uint8_t video_payload_type;
  uint8_t control_payload_type;
  uint16_t jitter_target_ms;
  uint16_t jitter_max_ms;
  int adaptation_enabled;
  uint32_t min_audio_bitrate_bps;
  uint32_t initial_audio_bitrate_bps;
  uint32_t max_audio_bitrate_bps;
  int enable_audio;
  int enable_video;
  mezia_camera_facing_t camera;
  uint32_t video_width;
  uint32_t video_height;
  uint32_t video_fps;
  uint32_t video_bitrate_bps;
  size_t max_nal_size;
  /* Optional native window / layer for decoded video (ANativeWindow* / CALayer*). */
  void *render_target;
  mezia_platform_error_callback_t on_error;
  mezia_platform_frame_callback_t on_decoded_frame;
  void *user_data;
} mezia_platform_config_t;

mezia_platform_t *mezia_platform_create(const mezia_platform_config_t *config);
void mezia_platform_destroy(mezia_platform_t *platform);
mezon_status_t mezia_platform_start(mezia_platform_t *platform);
mezon_status_t mezia_platform_stop(mezia_platform_t *platform);
mezon_status_t mezia_platform_set_video_bitrate(mezia_platform_t *platform,
                                                uint32_t bitrate_bps);
mezon_status_t mezia_platform_set_camera(mezia_platform_t *platform,
                                         mezia_camera_facing_t facing);
mezon_status_t mezia_platform_set_render_target(mezia_platform_t *platform,
                                                void *native_window);
void mezia_platform_get_stats(const mezia_platform_t *platform,
                              mezon_stats_t *stats);
mezia_ctx_t *mezia_platform_media(mezia_platform_t *platform);

#ifdef __cplusplus
}
#endif

#endif
