#ifndef MEZIA_AUDIO_INTERNAL_H
#define MEZIA_AUDIO_INTERNAL_H

#include "audio_feedback.h"
#include "bitrate_controller.h"
#include "mezia/audio.h"
#include "mezia/types.h"

#define MEZON_OPUS_MAX_PACKET_SIZE 1275U
#define MEZON_AUDIO_JITTER_SLOTS 8U

typedef struct mezon_opus_codec mezon_opus_codec_t;

typedef struct {
  uint8_t payload[MEZON_OPUS_MAX_PACKET_SIZE];
  size_t len;
  uint32_t timestamp;
  uint16_t seq;
  uint64_t arrival_time_ns;
  uint8_t marker;
  uint8_t occupied;
} mezon_audio_jitter_slot_t;

typedef struct {
  mezon_audio_jitter_slot_t slots[MEZON_AUDIO_JITTER_SLOTS];
  uint16_t expected_seq;
  uint32_t expected_timestamp;
  size_t count;
  size_t target_frames;
  size_t max_frames;
  uint64_t first_arrival_ns;
  int started;
  int initialized;
} mezon_audio_jitter_t;

mezon_opus_codec_t *mezon_opus_create(uint32_t initial_bitrate);
void mezon_opus_destroy(mezon_opus_codec_t *codec);
mezon_status_t mezon_opus_set_network_state(mezon_opus_codec_t *codec,
                                             uint32_t bitrate,
                                             uint8_t loss_percent);
mezon_status_t mezon_opus_encode(mezon_opus_codec_t *codec,
                                 const int16_t *pcm, uint8_t *output,
                                 size_t output_capacity, size_t *output_len);
mezon_status_t mezon_opus_decode(mezon_opus_codec_t *codec,
                                 const uint8_t *payload, size_t payload_len,
                                 int decode_fec, int16_t *pcm);

void mezon_audio_jitter_init(mezon_audio_jitter_t *jitter,
                             size_t target_frames, size_t max_frames);
mezon_status_t mezon_audio_jitter_insert(mezon_audio_jitter_t *jitter,
                                         const mezon_packet_t *packet,
                                         mezon_stats_t *stats);
int mezon_audio_jitter_ready(mezon_audio_jitter_t *jitter, uint64_t now_ns);
int mezon_audio_jitter_copy(mezon_audio_jitter_t *jitter, uint16_t seq,
                            uint8_t *payload, size_t *payload_len,
                            uint32_t *timestamp, uint8_t *marker, int remove);
void mezon_audio_jitter_advance(mezon_audio_jitter_t *jitter);

int mezon_audio_take_receiver_report(mezon_audio_ctx_t *ctx, uint64_t now_ns,
                                     mezon_audio_feedback_t *feedback);
mezon_status_t mezon_audio_submit_receiver_report(
    mezon_audio_ctx_t *ctx, const mezon_audio_feedback_t *feedback,
    uint64_t arrival_time_ns);
uint32_t mezon_audio_local_ssrc(const mezon_audio_ctx_t *ctx);
int mezon_audio_adaptation_enabled(const mezon_audio_ctx_t *ctx);
void mezon_audio_note_report_sent(mezon_audio_ctx_t *ctx);

#endif
