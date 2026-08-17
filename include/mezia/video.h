#ifndef mezia_VIDEO_H
#define mezia_VIDEO_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_video_ctx mezon_video_ctx_t;

mezon_video_ctx_t *mezon_video_create(int width, int height,
                                      int frame_interval_ms);
void mezon_video_destroy(mezon_video_ctx_t *ctx);

mezon_status_t mezon_video_packetize_nal(mezon_video_ctx_t *ctx,
                                         const uint8_t *nal_data,
                                         size_t nal_len, uint32_t rtp_timestamp,
                                         mezon_packet_t *out_packets,
                                         size_t *out_count);

size_t mezon_video_max_packets_for_nal(size_t nal_len);

mezon_status_t mezon_video_depacketize(mezon_video_ctx_t *ctx,
                                       const mezon_packet_t *in,
                                       uint8_t *nal_out, size_t nal_out_cap,
                                       size_t *nal_out_len,
                                       int *is_frame_complete);

#ifdef __cplusplus
}
#endif

#endif
