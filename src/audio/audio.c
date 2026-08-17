#include "mezia/audio.h"
#include "audio_internal.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct mezon_audio_ctx {
  mezon_audio_config_t config;
  mezon_opus_codec_t *codec;
  mezon_audio_jitter_t jitter;
  atomic_flag jitter_lock;
  uint16_t next_seq;
  uint32_t next_timestamp;
  int sender_silent;
  size_t consecutive_plc;
  uint8_t decode_payload[MEZON_OPUS_MAX_PACKET_SIZE];
  int16_t decode_buffer[MEZON_OPUS_FRAME_SAMPLES];
  mezon_stats_t stats;
};

static void lock_audio(mezon_audio_ctx_t *ctx) {
  while (atomic_flag_test_and_set_explicit(&ctx->jitter_lock,
                                            memory_order_acquire)) {
  }
}

static void unlock_audio(mezon_audio_ctx_t *ctx) {
  atomic_flag_clear_explicit(&ctx->jitter_lock, memory_order_release);
}

mezon_audio_ctx_t *mezon_audio_create(const mezon_audio_config_t *config) {
  mezon_audio_ctx_t *ctx;
  size_t target_frames;
  size_t max_frames;
  uint16_t target_ms;
  uint16_t max_ms;
  if (!config || config->payload_type > 127U ||
      (config->mtu && config->mtu <= MEZON_RTP_HEADER_SIZE + 2U)) {
    return NULL;
  }
  target_ms = config->jitter_target_ms ? config->jitter_target_ms : 60U;
  max_ms = config->jitter_max_ms ? config->jitter_max_ms : 120U;
  if (target_ms < 40U || max_ms > 120U || target_ms > max_ms ||
      target_ms % MEZON_OPUS_FRAME_MS != 0 ||
      max_ms % MEZON_OPUS_FRAME_MS != 0) {
    return NULL;
  }
  ctx = (mezon_audio_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }
  ctx->config = *config;
  ctx->config.mtu = config->mtu ? config->mtu : MEZON_DEFAULT_MTU;
  ctx->config.payload_type = config->payload_type
                                 ? config->payload_type
                                 : MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  ctx->config.jitter_target_ms = target_ms;
  ctx->config.jitter_max_ms = max_ms;
  ctx->codec = mezon_opus_create();
  if (!ctx->codec) {
    free(ctx);
    return NULL;
  }
  atomic_flag_clear(&ctx->jitter_lock);
  target_frames = target_ms / MEZON_OPUS_FRAME_MS;
  max_frames = max_ms / MEZON_OPUS_FRAME_MS;
  mezon_audio_jitter_init(&ctx->jitter, target_frames, max_frames);
  ctx->sender_silent = 1;
  return ctx;
}

void mezon_audio_destroy(mezon_audio_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  mezon_opus_destroy(ctx->codec);
  free(ctx);
}

size_t mezon_audio_max_packets(const mezon_audio_ctx_t *ctx,
                               size_t samples_per_channel) {
  return ctx && samples_per_channel == MEZON_OPUS_FRAME_SAMPLES ? 1U : 0U;
}

mezon_status_t mezon_audio_packetize(mezon_audio_ctx_t *ctx,
                                     const int16_t *pcm,
                                     size_t samples_per_channel,
                                     mezon_packet_t *packets,
                                     size_t packet_capacity,
                                     size_t *packet_count) {
  size_t max_payload;
  size_t encoded_len = 0;
  mezon_status_t status;
  int dtx_packet;
  if (!ctx || !pcm || !packets || !packet_count ||
      samples_per_channel != MEZON_OPUS_FRAME_SAMPLES) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (packet_capacity < 1U) {
    *packet_count = 1U;
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  max_payload = ctx->config.mtu - MEZON_RTP_HEADER_SIZE;
  if (max_payload > MEZON_OPUS_MAX_PACKET_SIZE) {
    max_payload = MEZON_OPUS_MAX_PACKET_SIZE;
  }
  if (!packets[0].data || packets[0].capacity < max_payload) {
    packets[0].len = max_payload;
    *packet_count = 1U;
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  status = mezon_opus_encode(ctx->codec, pcm, packets[0].data, max_payload,
                             &encoded_len);
  if (status != MEZON_OK) {
    return status;
  }
  dtx_packet = encoded_len <= 2U;
  packets[0].len = encoded_len;
  packets[0].payload_type = ctx->config.payload_type;
  packets[0].marker = (uint8_t)(!dtx_packet && ctx->sender_silent);
  packets[0].seq = ctx->next_seq;
  packets[0].timestamp = ctx->next_timestamp;
  packets[0].ssrc = ctx->config.ssrc;
  ctx->next_seq++;
  ctx->next_timestamp += MEZON_OPUS_FRAME_SAMPLES;
  ctx->sender_silent = dtx_packet;
  ctx->stats.audio_frames_encoded++;
  ctx->stats.packets_sent++;
  ctx->stats.bytes_sent += encoded_len;
  *packet_count = 1U;
  return MEZON_OK;
}

mezon_status_t mezon_audio_receive(mezon_audio_ctx_t *ctx,
                                   const mezon_packet_t *packet) {
  mezon_status_t status;
  if (!ctx || !packet || packet->payload_type != ctx->config.payload_type) {
    return MEZON_ERR_INVALID_ARG;
  }
  lock_audio(ctx);
  status = mezon_audio_jitter_insert(&ctx->jitter, packet, &ctx->stats);
  if (status == MEZON_OK) {
    ctx->stats.packets_received++;
    ctx->stats.bytes_received += packet->len;
  }
  unlock_audio(ctx);
  return status;
}

mezon_status_t mezon_audio_playout(mezon_audio_ctx_t *ctx, uint64_t now_ns) {
  size_t payload_len = 0;
  uint32_t timestamp = 0;
  uint8_t marker = 0;
  uint16_t expected_seq;
  int normal;
  int fec;
  int discontinuity = 0;
  mezon_status_t status;
  if (!ctx) {
    return MEZON_ERR_INVALID_ARG;
  }
  lock_audio(ctx);
  if (!mezon_audio_jitter_ready(&ctx->jitter, now_ns)) {
    unlock_audio(ctx);
    return MEZON_ERR_NOT_READY;
  }
  expected_seq = ctx->jitter.expected_seq;
  timestamp = ctx->jitter.expected_timestamp;
  normal = mezon_audio_jitter_copy(&ctx->jitter, expected_seq,
                                   ctx->decode_payload, &payload_len, &timestamp,
                                   &marker, 1);
  fec = 0;
  if (!normal) {
    fec = mezon_audio_jitter_copy(&ctx->jitter, (uint16_t)(expected_seq + 1U),
                                  ctx->decode_payload, &payload_len, &timestamp,
                                  &marker, 0);
    timestamp = ctx->jitter.expected_timestamp;
    ctx->stats.sequence_gaps++;
    ctx->stats.audio_jitter_underruns++;
  }
  mezon_audio_jitter_advance(&ctx->jitter);
  unlock_audio(ctx);

  if (normal) {
    status = mezon_opus_decode(ctx->codec, ctx->decode_payload, payload_len, 0,
                               ctx->decode_buffer);
    ctx->consecutive_plc = 0;
  } else if (fec) {
    status = mezon_opus_decode(ctx->codec, ctx->decode_payload, payload_len, 1,
                               ctx->decode_buffer);
    if (status == MEZON_OK) {
      ctx->stats.audio_frames_fec_recovered++;
    }
    ctx->consecutive_plc = 0;
  } else if (ctx->consecutive_plc < 6U) {
    status = mezon_opus_decode(ctx->codec, NULL, 0, 0, ctx->decode_buffer);
    if (status == MEZON_OK) {
      ctx->stats.audio_frames_plc++;
    }
    ctx->consecutive_plc++;
    discontinuity = 1;
  } else {
    memset(ctx->decode_buffer, 0, sizeof(ctx->decode_buffer));
    status = MEZON_OK;
    discontinuity = 1;
  }
  if (status != MEZON_OK) {
    ctx->stats.audio_frames_dropped++;
    return status;
  }
  ctx->stats.audio_frames_decoded++;
  if (ctx->config.on_audio) {
    ctx->config.on_audio(ctx->decode_buffer, MEZON_OPUS_FRAME_SAMPLES,
                         timestamp, discontinuity, ctx->config.user_data);
  }
  return MEZON_OK;
}

void mezon_audio_get_stats(const mezon_audio_ctx_t *ctx, mezon_stats_t *stats) {
  mezon_audio_ctx_t *mutable_ctx = (mezon_audio_ctx_t *)ctx;
  if (!ctx || !stats) {
    return;
  }
  lock_audio(mutable_ctx);
  *stats = ctx->stats;
  unlock_audio(mutable_ctx);
}
