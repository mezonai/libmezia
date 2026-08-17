#ifndef MEZIA_AUDIO_H
#define MEZIA_AUDIO_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_audio_ctx mezon_audio_ctx_t;

/* Callback runs on the thread calling mezon_audio_playout(). PCM is always
 * 48 kHz, mono, signed 16-bit, with 960 samples per normal frame. */
typedef void (*mezon_audio_callback_t)(const int16_t *pcm,
                                       size_t samples_per_channel,
                                       uint32_t rtp_timestamp, int discontinuity,
                                       void *user_data);

typedef struct {
  int enabled;
  uint32_t min_bitrate_bps;
  uint32_t initial_bitrate_bps;
  uint32_t max_bitrate_bps;
  uint16_t report_interval_ms;
  uint16_t feedback_timeout_ms;
} mezon_audio_adaptation_config_t;

typedef struct {
  uint8_t payload_type;
  uint32_t ssrc;
  size_t mtu;
  uint16_t jitter_target_ms;
  uint16_t jitter_max_ms;
  mezon_audio_adaptation_config_t adaptation;
  mezon_audio_callback_t on_audio;
  void *user_data;
} mezon_audio_config_t;

mezon_audio_ctx_t *mezon_audio_create(const mezon_audio_config_t *config);
void mezon_audio_destroy(mezon_audio_ctx_t *ctx);
size_t mezon_audio_max_packets(const mezon_audio_ctx_t *ctx,
                               size_t samples_per_channel);
mezon_status_t mezon_audio_packetize(mezon_audio_ctx_t *ctx,
                                     const int16_t *pcm,
                                     size_t samples_per_channel,
                                     mezon_packet_t *packets,
                                     size_t packet_capacity,
                                     size_t *packet_count);
/* Copies compressed packet data into bounded context-owned jitter storage. */
mezon_status_t mezon_audio_receive(mezon_audio_ctx_t *ctx,
                                   const mezon_packet_t *packet);
/* Call once every 20 ms. Returns NOT_READY while initial jitter buffering. */
mezon_status_t mezon_audio_playout(mezon_audio_ctx_t *ctx, uint64_t now_ns);
void mezon_audio_get_stats(const mezon_audio_ctx_t *ctx, mezon_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
