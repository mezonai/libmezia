#ifndef MEZIA_VIDEO_H
#define MEZIA_VIDEO_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_video_ctx mezon_video_ctx_t;

typedef void (*mezon_video_callback_t)(const uint8_t *nal, size_t nal_len,
                                       uint32_t rtp_timestamp,
                                       int end_of_access_unit,
                                       int discontinuity, void *user_data);

typedef struct {
  uint8_t payload_type;
  uint32_t ssrc;
  size_t mtu;
  size_t max_nal_size;
  mezon_video_callback_t on_nal;
  void *user_data;
} mezon_video_config_t;

mezon_video_ctx_t *mezon_video_create(const mezon_video_config_t *config);
void mezon_video_destroy(mezon_video_ctx_t *ctx);
size_t mezon_video_max_packets_for_nal(const mezon_video_ctx_t *ctx,
                                       size_t nal_len);
mezon_status_t mezon_video_packetize_nal(mezon_video_ctx_t *ctx,
                                         const uint8_t *nal_data,
                                         size_t nal_len,
                                         uint32_t rtp_timestamp,
                                         int end_of_access_unit,
                                         mezon_packet_t *packets,
                                         size_t packet_capacity,
                                         size_t *packet_count);
mezon_status_t mezon_video_receive(mezon_video_ctx_t *ctx,
                                   const mezon_packet_t *packet);
void mezon_video_get_stats(const mezon_video_ctx_t *ctx, mezon_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
