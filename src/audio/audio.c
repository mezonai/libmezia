#include "mezia/audio.h"
#include "mezia_internal.h"

#include <stdlib.h>
#include <string.h>

struct mezon_audio_ctx {
  mezon_audio_config_t config;
  uint16_t next_seq;
  uint32_t next_timestamp;
  uint16_t expected_seq;
  int have_expected_seq;
  int16_t *decode_buffer;
  size_t decode_samples;
  mezon_stats_t stats;
};

mezon_audio_ctx_t *mezon_audio_create(const mezon_audio_config_t *config) {
  mezon_audio_ctx_t *ctx;
  size_t max_payload;
  if (!config || config->sample_rate == 0 || config->channels == 0 ||
      config->payload_type > 127U ||
      (config->mtu && config->mtu <= MEZON_RTP_HEADER_SIZE + 2U)) {
    return NULL;
  }
  ctx = (mezon_audio_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }
  ctx->config = *config;
  ctx->config.mtu = config->mtu ? config->mtu : MEZON_DEFAULT_MTU;
  if (!ctx->config.payload_type) {
    ctx->config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  }
  max_payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  ctx->decode_samples = max_payload / 2U;
  ctx->decode_buffer =
      (int16_t *)malloc(ctx->decode_samples * sizeof(*ctx->decode_buffer));
  if (!ctx->decode_buffer) {
    free(ctx);
    return NULL;
  }
  return ctx;
}

void mezon_audio_destroy(mezon_audio_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  free(ctx->decode_buffer);
  free(ctx);
}

size_t mezon_audio_max_packets(const mezon_audio_ctx_t *ctx,
                               size_t samples_per_channel) {
  size_t bytes;
  size_t payload;
  if (!ctx || !samples_per_channel ||
      samples_per_channel > SIZE_MAX / ctx->config.channels / sizeof(int16_t)) {
    return 0;
  }
  bytes = samples_per_channel * ctx->config.channels * sizeof(int16_t);
  payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  payload -= payload % (ctx->config.channels * sizeof(int16_t));
  return payload ? bytes / payload + (bytes % payload != 0) : 0;
}

mezon_status_t mezon_audio_packetize(mezon_audio_ctx_t *ctx,
                                     const int16_t *pcm,
                                     size_t samples_per_channel,
                                     mezon_packet_t *packets,
                                     size_t packet_capacity,
                                     size_t *packet_count) {
  size_t required;
  size_t frame_bytes;
  size_t max_payload;
  size_t sample_offset = 0;
  size_t i;
  if (!ctx || !pcm || !packets || !packet_count || !samples_per_channel) {
    return MEZON_ERR_INVALID_ARG;
  }
  required = mezon_audio_max_packets(ctx, samples_per_channel);
  if (packet_capacity < required) {
    *packet_count = required;
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  frame_bytes = ctx->config.channels * sizeof(int16_t);
  max_payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  max_payload -= max_payload % frame_bytes;
  {
    size_t remaining = samples_per_channel;
    for (i = 0; i < required; ++i) {
      size_t frames = max_payload / frame_bytes;
      if (frames > remaining) {
        frames = remaining;
      }
      if (!packets[i].data || packets[i].capacity < frames * frame_bytes) {
        packets[i].len = frames * frame_bytes;
        *packet_count = required;
        return MEZON_ERR_BUFFER_TOO_SMALL;
      }
      remaining -= frames;
    }
  }
  for (i = 0; i < required; ++i) {
    size_t samples_left = samples_per_channel - sample_offset;
    size_t frames = max_payload / frame_bytes;
    size_t j;
    if (frames > samples_left) {
      frames = samples_left;
    }
    for (j = 0; j < frames * ctx->config.channels; ++j) {
      uint16_t value = (uint16_t)pcm[sample_offset * ctx->config.channels + j];
      packets[i].data[j * 2U] = (uint8_t)(value >> 8);
      packets[i].data[j * 2U + 1U] = (uint8_t)value;
    }
    packets[i].len = frames * frame_bytes;
    packets[i].payload_type = ctx->config.payload_type;
    packets[i].marker = (uint8_t)(i + 1U == required);
    packets[i].seq = ctx->next_seq++;
    packets[i].timestamp = ctx->next_timestamp + (uint32_t)sample_offset;
    packets[i].ssrc = ctx->config.ssrc;
    sample_offset += frames;
  }
  ctx->next_timestamp += (uint32_t)samples_per_channel;
  *packet_count = required;
  return MEZON_OK;
}

mezon_status_t mezon_audio_receive(mezon_audio_ctx_t *ctx,
                                   const mezon_packet_t *packet) {
  size_t scalar_samples;
  size_t samples_per_channel;
  size_t i;
  int discontinuity = 0;
  if (!ctx || !packet || !packet->data ||
      packet->payload_type != ctx->config.payload_type ||
      packet->len % (ctx->config.channels * sizeof(int16_t)) != 0) {
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
      discontinuity = 1;
    }
  }
  ctx->expected_seq = (uint16_t)(packet->seq + 1U);
  ctx->have_expected_seq = 1;
  scalar_samples = packet->len / 2U;
  if (scalar_samples > ctx->decode_samples) {
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  for (i = 0; i < scalar_samples; ++i) {
    ctx->decode_buffer[i] =
        (int16_t)(((uint16_t)packet->data[i * 2U] << 8) |
                  packet->data[i * 2U + 1U]);
  }
  samples_per_channel = scalar_samples / ctx->config.channels;
  ctx->stats.packets_received++;
  ctx->stats.bytes_received += packet->len;
  if (ctx->config.on_audio) {
    ctx->config.on_audio(ctx->decode_buffer, samples_per_channel,
                         packet->timestamp, discontinuity,
                         ctx->config.user_data);
  }
  return MEZON_OK;
}

void mezon_audio_get_stats(const mezon_audio_ctx_t *ctx, mezon_stats_t *stats) {
  if (ctx && stats) {
    *stats = ctx->stats;
  }
}
