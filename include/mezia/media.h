#ifndef MEZIA_MEDIA_H
#define MEZIA_MEDIA_H

#include "mezia/audio.h"
#include "mezia/peer.h"
#include "mezia/video.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezia_ctx mezia_ctx_t;

typedef struct {
  mezon_peer_config_t peer;
  const mezon_audio_config_t *audio;
  const mezon_video_config_t *video;
} mezia_config_t;

mezia_ctx_t *mezia_create(const mezia_config_t *config);
void mezia_destroy(mezia_ctx_t *ctx);
mezon_status_t mezia_start(mezia_ctx_t *ctx);
mezon_status_t mezia_stop(mezia_ctx_t *ctx);
/* pcm must contain exactly 960 mono samples (20 ms at 48 kHz). */
mezon_status_t mezia_send_audio(mezia_ctx_t *ctx, const int16_t *pcm,
                                size_t samples_per_channel);
mezon_status_t mezia_playout_audio(mezia_ctx_t *ctx, uint64_t now_ns);
mezon_status_t mezia_send_h264(mezia_ctx_t *ctx, const uint8_t *nal,
                               size_t nal_len, uint32_t rtp_timestamp,
                               int end_of_access_unit);
void mezia_get_stats(const mezia_ctx_t *ctx, mezon_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
