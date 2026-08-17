#include "mezia/video.h"

struct mezon_video_ctx {
  int width;
  int height;
  int frame_interval_ms;
};

mezon_video_ctx_t *mezon_video_create(int width, int height,
                                      int frame_interval_ms) {
  (void)width;
  (void)height;
  (void)frame_interval_ms;
  return NULL;
}

void mezon_video_destroy(mezon_video_ctx_t *ctx) { (void)ctx; }

size_t mezon_video_max_packets_for_nal(size_t nal_len) {
  (void)nal_len;
  return 0;
}

mezon_status_t mezon_video_packetize_nal(mezon_video_ctx_t *ctx,
                                         const uint8_t *nal_data,
                                         size_t nal_len, uint32_t rtp_timestamp,
                                         mezon_packet_t *out_packets,
                                         size_t *out_count) {
  (void)ctx;
  (void)nal_data;
  (void)nal_len;
  (void)rtp_timestamp;
  (void)out_packets;
  (void)out_count;
  return MEZON_ERR_NOT_READY;
}

mezon_status_t mezon_video_depacketize(mezon_video_ctx_t *ctx,
                                       const mezon_packet_t *in,
                                       uint8_t *nal_out, size_t nal_out_cap,
                                       size_t *nal_out_len,
                                       int *is_frame_complete) {
  (void)ctx;
  (void)in;
  (void)nal_out;
  (void)nal_out_cap;
  (void)nal_out_len;
  (void)is_frame_complete;
  return MEZON_ERR_NOT_READY;
}
