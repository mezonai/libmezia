#ifndef MEZIA_AUDIO_H
#define MEZIA_AUDIO_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_audio_ctx mezon_audio_ctx_t;

typedef void (*mezon_audio_callback_t)(const int16_t *pcm,
                                       size_t samples_per_channel,
                                       uint32_t rtp_timestamp, int discontinuity,
                                       void *user_data);

typedef struct {
  uint32_t sample_rate;
  uint16_t channels;
  uint8_t payload_type;
  uint32_t ssrc;
  size_t mtu;
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
mezon_status_t mezon_audio_receive(mezon_audio_ctx_t *ctx,
                                   const mezon_packet_t *packet);
void mezon_audio_get_stats(const mezon_audio_ctx_t *ctx, mezon_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
