#include "mezia/audio.h"
#include "audio_internal.h"
#include "mezia_internal.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct mezon_audio_ctx {
  mezon_audio_config_t config;
  mezon_opus_codec_t *codec;
  mezon_audio_jitter_t jitter;
  atomic_flag jitter_lock;
  atomic_flag feedback_lock;
  mezon_bitrate_controller_t controller;
  mezon_audio_feedback_t pending_feedback;
  uint64_t pending_feedback_time_ns;
  uint64_t last_feedback_time_ns;
  uint64_t report_started_ns;
  uint64_t report_seen_mask;
  uint32_t report_ssrc;
  uint16_t report_base_seq;
  uint16_t report_highest_seq;
  uint16_t next_report_seq;
  uint16_t last_feedback_seq;
  int adaptation_enabled;
  int feedback_pending;
  int have_feedback_seq;
  int stale_backoff_applied;
  int report_initialized;
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

static void lock_feedback(mezon_audio_ctx_t *ctx) {
  while (atomic_flag_test_and_set_explicit(&ctx->feedback_lock,
                                            memory_order_acquire)) {
  }
}

static void unlock_feedback(mezon_audio_ctx_t *ctx) {
  atomic_flag_clear_explicit(&ctx->feedback_lock, memory_order_release);
}

static void apply_pending_feedback(mezon_audio_ctx_t *ctx) {
  mezon_audio_feedback_t feedback;
  mezon_bitrate_decision_t decision;
  int pending = 0;
  if (!ctx->adaptation_enabled) {
    return;
  }
  lock_feedback(ctx);
  if (ctx->feedback_pending) {
    feedback = ctx->pending_feedback;
    ctx->feedback_pending = 0;
    pending = 1;
  }
  unlock_feedback(ctx);
  if (!pending) {
    uint64_t now_ns = mezon_clock_now_ns();
    uint64_t timeout_ns =
        (uint64_t)ctx->config.adaptation.feedback_timeout_ms * 1000000ULL;
    if (!ctx->have_feedback_seq || ctx->stale_backoff_applied ||
        now_ns < ctx->last_feedback_time_ns ||
        now_ns - ctx->last_feedback_time_ns <= timeout_ns) {
      return;
    }
    feedback.expected = 100U;
    feedback.received = 90U;
    ctx->stale_backoff_applied = 1;
    ctx->stats.audio_adaptation_stale_events++;
  } else {
    ctx->stale_backoff_applied = 0;
  }
  decision = mezon_bitrate_controller_update(&ctx->controller,
                                              feedback.expected,
                                              feedback.received);
  if (mezon_opus_set_network_state(ctx->codec, decision.bitrate,
                                   decision.loss_percent) == MEZON_OK) {
    ctx->stats.audio_current_bitrate_bps = decision.bitrate;
    ctx->stats.audio_current_packet_loss_percent = decision.loss_percent;
    ctx->stats.audio_bitrate_increases += (uint64_t)decision.increased;
    ctx->stats.audio_bitrate_decreases += (uint64_t)decision.decreased;
  }
}

mezon_audio_ctx_t *mezon_audio_create(const mezon_audio_config_t *config) {
  mezon_audio_ctx_t *ctx;
  size_t target_frames;
  size_t max_frames;
  uint16_t target_ms;
  uint16_t max_ms;
  uint32_t min_bitrate = 16000U;
  uint32_t initial_bitrate = MEZON_OPUS_BITRATE;
  uint32_t max_bitrate = 48000U;
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
  if (config->adaptation.enabled) {
    min_bitrate = config->adaptation.min_bitrate_bps
                      ? config->adaptation.min_bitrate_bps
                      : 16000U;
    initial_bitrate = config->adaptation.initial_bitrate_bps
                          ? config->adaptation.initial_bitrate_bps
                          : MEZON_OPUS_BITRATE;
    max_bitrate = config->adaptation.max_bitrate_bps
                      ? config->adaptation.max_bitrate_bps
                      : 48000U;
    if (min_bitrate < 16000U || max_bitrate > 48000U ||
        min_bitrate > initial_bitrate || initial_bitrate > max_bitrate) {
      return NULL;
    }
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
  ctx->config.adaptation.min_bitrate_bps = min_bitrate;
  ctx->config.adaptation.initial_bitrate_bps = initial_bitrate;
  ctx->config.adaptation.max_bitrate_bps = max_bitrate;
  if (!ctx->config.adaptation.report_interval_ms) {
    ctx->config.adaptation.report_interval_ms = 1000U;
  }
  if (!ctx->config.adaptation.feedback_timeout_ms) {
    ctx->config.adaptation.feedback_timeout_ms = 3000U;
  }
  if (ctx->config.adaptation.enabled &&
      (ctx->config.adaptation.report_interval_ms < 200U ||
       ctx->config.adaptation.feedback_timeout_ms <
           ctx->config.adaptation.report_interval_ms * 2U)) {
    free(ctx);
    return NULL;
  }
  ctx->codec = mezon_opus_create(initial_bitrate);
  if (!ctx->codec) {
    free(ctx);
    return NULL;
  }
  atomic_flag_clear(&ctx->jitter_lock);
  atomic_flag_clear(&ctx->feedback_lock);
  ctx->adaptation_enabled = config->adaptation.enabled;
  mezon_bitrate_controller_init(&ctx->controller, min_bitrate, initial_bitrate,
                                 max_bitrate);
  ctx->stats.audio_current_bitrate_bps = initial_bitrate;
  ctx->stats.audio_current_packet_loss_percent =
      MEZON_OPUS_EXPECTED_LOSS_PERCENT;
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
  apply_pending_feedback(ctx);
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

static void track_receiver_packet(mezon_audio_ctx_t *ctx,
                                  const mezon_packet_t *packet) {
  uint16_t distance;
  if (!ctx->adaptation_enabled) {
    return;
  }
  if (!ctx->report_initialized || packet->ssrc != ctx->report_ssrc) {
    ctx->report_ssrc = packet->ssrc;
    ctx->report_base_seq = packet->seq;
    ctx->report_highest_seq = packet->seq;
    ctx->report_seen_mask = 1U;
    ctx->report_started_ns = packet->arrival_time_ns;
    ctx->report_initialized = 1;
    return;
  }
  distance = (uint16_t)(packet->seq - ctx->report_base_seq);
  if (distance < 64U) {
    ctx->report_seen_mask |= 1ULL << distance;
    if (mezon_seq_before(ctx->report_highest_seq, packet->seq)) {
      ctx->report_highest_seq = packet->seq;
    }
  }
}

mezon_status_t mezon_audio_receive(mezon_audio_ctx_t *ctx,
                                   const mezon_packet_t *packet) {
  mezon_status_t status;
  if (!ctx || !packet || packet->payload_type != ctx->config.payload_type) {
    return MEZON_ERR_INVALID_ARG;
  }
  lock_audio(ctx);
  track_receiver_packet(ctx, packet);
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

static uint32_t count_bits(uint64_t value) {
  uint32_t count = 0;
  while (value) {
    count += (uint32_t)(value & 1U);
    value >>= 1;
  }
  return count;
}

int mezon_audio_take_receiver_report(mezon_audio_ctx_t *ctx, uint64_t now_ns,
                                     mezon_audio_feedback_t *feedback) {
  uint64_t interval_ns;
  if (!ctx || !feedback || !ctx->adaptation_enabled) {
    return 0;
  }
  interval_ns =
      (uint64_t)ctx->config.adaptation.report_interval_ms * 1000000ULL;
  lock_audio(ctx);
  if (!ctx->report_initialized || now_ns < ctx->report_started_ns ||
      now_ns - ctx->report_started_ns < interval_ns) {
    unlock_audio(ctx);
    return 0;
  }
  feedback->report_seq = ctx->next_report_seq++;
  feedback->media_ssrc = ctx->report_ssrc;
  feedback->expected =
      (uint16_t)(ctx->report_highest_seq - ctx->report_base_seq) + 1U;
  feedback->received = count_bits(ctx->report_seen_mask);
  ctx->report_initialized = 0;
  ctx->report_seen_mask = 0;
  unlock_audio(ctx);
  return feedback->expected != 0;
}

mezon_status_t mezon_audio_submit_receiver_report(
    mezon_audio_ctx_t *ctx, const mezon_audio_feedback_t *feedback,
    uint64_t arrival_time_ns) {
  if (!ctx || !feedback || !ctx->adaptation_enabled || !feedback->expected ||
      feedback->received > feedback->expected ||
      feedback->media_ssrc != ctx->config.ssrc) {
    return MEZON_ERR_INVALID_ARG;
  }
  lock_feedback(ctx);
  if (ctx->have_feedback_seq &&
      !mezon_seq_before(ctx->last_feedback_seq, feedback->report_seq)) {
    unlock_feedback(ctx);
    ctx->stats.audio_adaptation_reports_rejected++;
    return MEZON_ERR_STATE;
  }
  ctx->pending_feedback = *feedback;
  ctx->pending_feedback_time_ns = arrival_time_ns;
  ctx->last_feedback_time_ns = arrival_time_ns;
  ctx->last_feedback_seq = feedback->report_seq;
  ctx->have_feedback_seq = 1;
  ctx->feedback_pending = 1;
  unlock_feedback(ctx);
  ctx->stats.audio_adaptation_reports_received++;
  return MEZON_OK;
}

uint32_t mezon_audio_local_ssrc(const mezon_audio_ctx_t *ctx) {
  return ctx ? ctx->config.ssrc : 0;
}

int mezon_audio_adaptation_enabled(const mezon_audio_ctx_t *ctx) {
  return ctx && ctx->adaptation_enabled;
}

void mezon_audio_note_report_sent(mezon_audio_ctx_t *ctx) {
  if (ctx) {
    ctx->stats.audio_adaptation_reports_sent++;
  }
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
