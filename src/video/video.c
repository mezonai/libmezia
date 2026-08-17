#include "mezia/video.h"
#include "mezia_internal.h"

#include <stdlib.h>
#include <string.h>

struct mezon_video_ctx {
  mezon_video_config_t config;
  uint16_t next_seq;
  uint16_t expected_seq;
  int have_expected_seq;
  uint8_t *reassembly;
  size_t reassembly_len;
  uint32_t reassembly_timestamp;
  int reassembling;
  int discontinuity;
  mezon_stats_t stats;
};

static int valid_nal(const uint8_t *nal, size_t nal_len) {
  uint8_t type;
  if (!nal || nal_len == 0 || (nal[0] & 0x80U)) {
    return 0;
  }
  type = (uint8_t)(nal[0] & 0x1fU);
  return type >= 1U && type <= 23U;
}

mezon_video_ctx_t *mezon_video_create(const mezon_video_config_t *config) {
  mezon_video_ctx_t *ctx;
  if (!config || config->payload_type > 127U ||
      (config->mtu && config->mtu <= MEZON_RTP_HEADER_SIZE + 2U)) {
    return NULL;
  }
  ctx = (mezon_video_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }
  ctx->config = *config;
  ctx->config.mtu = config->mtu ? config->mtu : MEZON_DEFAULT_MTU;
  ctx->config.max_nal_size =
      config->max_nal_size ? config->max_nal_size : 2U * 1024U * 1024U;
  if (!ctx->config.payload_type) {
    ctx->config.payload_type = MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE;
  }
  ctx->reassembly = (uint8_t *)malloc(ctx->config.max_nal_size);
  if (!ctx->reassembly) {
    free(ctx);
    return NULL;
  }
  return ctx;
}

void mezon_video_destroy(mezon_video_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  free(ctx->reassembly);
  free(ctx);
}

size_t mezon_video_max_packets_for_nal(const mezon_video_ctx_t *ctx,
                                       size_t nal_len) {
  size_t payload;
  size_t fragment;
  if (!ctx || !nal_len) {
    return 0;
  }
  payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  if (nal_len <= payload) {
    return 1;
  }
  fragment = payload - 2U;
  return fragment ? (nal_len - 1U) / fragment +
                        ((nal_len - 1U) % fragment != 0)
                  : 0;
}

mezon_status_t mezon_video_packetize_nal(mezon_video_ctx_t *ctx,
                                         const uint8_t *nal_data,
                                         size_t nal_len,
                                         uint32_t rtp_timestamp,
                                         int end_of_access_unit,
                                         mezon_packet_t *packets,
                                         size_t packet_capacity,
                                         size_t *packet_count) {
  size_t required;
  size_t payload;
  size_t i;
  if (!ctx || !valid_nal(nal_data, nal_len) || !packets || !packet_count) {
    return MEZON_ERR_INVALID_ARG;
  }
  required = mezon_video_max_packets_for_nal(ctx, nal_len);
  if (packet_capacity < required) {
    *packet_count = required;
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  if (required == 1U) {
    if (!packets[0].data || packets[0].capacity < nal_len) {
      packets[0].len = nal_len;
      *packet_count = 1;
      return MEZON_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(packets[0].data, nal_data, nal_len);
    packets[0].len = nal_len;
    packets[0].payload_type = ctx->config.payload_type;
    packets[0].marker = (uint8_t)(end_of_access_unit != 0);
    packets[0].seq = ctx->next_seq++;
    packets[0].timestamp = rtp_timestamp;
    packets[0].ssrc = ctx->config.ssrc;
  } else {
    uint8_t fu_indicator = (uint8_t)((nal_data[0] & 0xe0U) | 28U);
    uint8_t nal_type = (uint8_t)(nal_data[0] & 0x1fU);
    size_t fragment = payload - 2U;
    size_t offset = 1U;
    size_t validate_offset = 1U;
    for (i = 0; i < required; ++i) {
      size_t bytes = nal_len - validate_offset;
      if (bytes > fragment) {
        bytes = fragment;
      }
      if (!packets[i].data || packets[i].capacity < bytes + 2U) {
        packets[i].len = bytes + 2U;
        *packet_count = required;
        return MEZON_ERR_BUFFER_TOO_SMALL;
      }
      validate_offset += bytes;
    }
    for (i = 0; i < required; ++i) {
      size_t bytes = nal_len - offset;
      if (bytes > fragment) {
        bytes = fragment;
      }
      packets[i].data[0] = fu_indicator;
      packets[i].data[1] = nal_type;
      if (i == 0) {
        packets[i].data[1] |= 0x80U;
      }
      if (i + 1U == required) {
        packets[i].data[1] |= 0x40U;
      }
      memcpy(packets[i].data + 2U, nal_data + offset, bytes);
      packets[i].len = bytes + 2U;
      packets[i].payload_type = ctx->config.payload_type;
      packets[i].marker =
          (uint8_t)(end_of_access_unit && i + 1U == required);
      packets[i].seq = ctx->next_seq++;
      packets[i].timestamp = rtp_timestamp;
      packets[i].ssrc = ctx->config.ssrc;
      offset += bytes;
    }
  }
  *packet_count = required;
  return MEZON_OK;
}

static void reset_reassembly(mezon_video_ctx_t *ctx, int failed) {
  if (failed && ctx->reassembling) {
    ctx->stats.reassembly_failures++;
  }
  ctx->reassembling = 0;
  ctx->reassembly_len = 0;
}

mezon_status_t mezon_video_receive(mezon_video_ctx_t *ctx,
                                   const mezon_packet_t *packet) {
  uint8_t type;
  int discontinuity = 0;
  if (!ctx || !packet || !packet->data || packet->len == 0 ||
      packet->payload_type != ctx->config.payload_type) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (ctx->have_expected_seq) {
    if (packet->seq == (uint16_t)(ctx->expected_seq - 1U)) {
      ctx->stats.duplicate_packets++;
      return MEZON_OK;
    }
    if (mezon_seq_before(packet->seq, ctx->expected_seq)) {
      ctx->stats.late_packets++;
      return MEZON_OK;
    }
    if (packet->seq != ctx->expected_seq) {
      ctx->stats.sequence_gaps += (uint16_t)(packet->seq - ctx->expected_seq);
      reset_reassembly(ctx, 1);
      discontinuity = 1;
    }
  }
  ctx->expected_seq = (uint16_t)(packet->seq + 1U);
  ctx->have_expected_seq = 1;
  type = (uint8_t)(packet->data[0] & 0x1fU);
  if (type >= 1U && type <= 23U) {
    reset_reassembly(ctx, 1);
    if (ctx->config.on_nal) {
      ctx->config.on_nal(packet->data, packet->len, packet->timestamp,
                         packet->marker, discontinuity, ctx->config.user_data);
    }
  } else if (type == 28U) {
    uint8_t header;
    int start;
    int end;
    size_t bytes;
    if (packet->len < 3U) {
      ctx->stats.malformed_packets++;
      reset_reassembly(ctx, 1);
      return MEZON_ERR_MALFORMED_PACKET;
    }
    header = packet->data[1];
    start = (header & 0x80U) != 0;
    end = (header & 0x40U) != 0;
    if ((header & 0x20U) || (start && end)) {
      ctx->stats.malformed_packets++;
      reset_reassembly(ctx, 1);
      return MEZON_ERR_MALFORMED_PACKET;
    }
    bytes = packet->len - 2U;
    if (start) {
      reset_reassembly(ctx, 1);
      ctx->reassembly[0] =
          (uint8_t)((packet->data[0] & 0xe0U) | (header & 0x1fU));
      ctx->reassembly_len = 1U;
      ctx->reassembly_timestamp = packet->timestamp;
      ctx->reassembling = 1;
      ctx->discontinuity = discontinuity;
    } else if (!ctx->reassembling ||
               ctx->reassembly_timestamp != packet->timestamp) {
      ctx->stats.reassembly_failures++;
      return MEZON_ERR_MALFORMED_PACKET;
    }
    if (ctx->reassembly_len + bytes > ctx->config.max_nal_size) {
      reset_reassembly(ctx, 1);
      return MEZON_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(ctx->reassembly + ctx->reassembly_len, packet->data + 2U, bytes);
    ctx->reassembly_len += bytes;
    if (end) {
      if (ctx->config.on_nal) {
        ctx->config.on_nal(ctx->reassembly, ctx->reassembly_len,
                           packet->timestamp, packet->marker,
                           ctx->discontinuity, ctx->config.user_data);
      }
      reset_reassembly(ctx, 0);
    }
  } else {
    ctx->stats.malformed_packets++;
    return MEZON_ERR_UNSUPPORTED;
  }
  ctx->stats.packets_received++;
  ctx->stats.bytes_received += packet->len;
  return MEZON_OK;
}

void mezon_video_get_stats(const mezon_video_ctx_t *ctx, mezon_stats_t *stats) {
  if (ctx && stats) {
    *stats = ctx->stats;
  }
}
