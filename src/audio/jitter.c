#include "audio_internal.h"
#include "mezia_internal.h"

#include <string.h>

void mezon_audio_jitter_init(mezon_audio_jitter_t *jitter,
                             size_t target_frames, size_t max_frames) {
  memset(jitter, 0, sizeof(*jitter));
  jitter->target_frames = target_frames;
  jitter->max_frames = max_frames;
}

static void reset_jitter(mezon_audio_jitter_t *jitter,
                         const mezon_packet_t *packet,
                         mezon_stats_t *stats) {
  size_t target = jitter->target_frames;
  size_t maximum = jitter->max_frames;
  memset(jitter, 0, sizeof(*jitter));
  jitter->target_frames = target;
  jitter->max_frames = maximum;
  jitter->expected_seq = packet->seq;
  jitter->expected_timestamp = packet->timestamp;
  jitter->first_arrival_ns = packet->arrival_time_ns;
  jitter->initialized = 1;
  stats->audio_jitter_resets++;
}

mezon_status_t mezon_audio_jitter_insert(mezon_audio_jitter_t *jitter,
                                         const mezon_packet_t *packet,
                                         mezon_stats_t *stats) {
  mezon_audio_jitter_slot_t *slot;
  uint16_t distance;
  uint32_t expected_timestamp;
  if (!jitter || !packet || !packet->data || !packet->len || !stats ||
      packet->len > MEZON_OPUS_MAX_PACKET_SIZE) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (!jitter->initialized) {
    reset_jitter(jitter, packet, stats);
    stats->audio_jitter_resets--;
  }
  if (mezon_seq_before(packet->seq, jitter->expected_seq)) {
    stats->late_packets++;
    stats->audio_frames_dropped++;
    return MEZON_OK;
  }
  distance = (uint16_t)(packet->seq - jitter->expected_seq);
  if (distance >= MEZON_AUDIO_JITTER_SLOTS || distance >= jitter->max_frames) {
    reset_jitter(jitter, packet, stats);
    distance = 0;
  }
  expected_timestamp =
      jitter->expected_timestamp + (uint32_t)distance * MEZON_OPUS_FRAME_SAMPLES;
  if (packet->timestamp != expected_timestamp) {
    stats->malformed_packets++;
    return MEZON_ERR_MALFORMED_PACKET;
  }
  slot = &jitter->slots[packet->seq % MEZON_AUDIO_JITTER_SLOTS];
  if (slot->occupied) {
    if (slot->seq == packet->seq) {
      stats->duplicate_packets++;
      return MEZON_OK;
    }
    stats->audio_frames_dropped++;
  } else {
    jitter->count++;
  }
  memcpy(slot->payload, packet->data, packet->len);
  slot->len = packet->len;
  slot->timestamp = packet->timestamp;
  slot->seq = packet->seq;
  slot->arrival_time_ns = packet->arrival_time_ns;
  slot->marker = packet->marker;
  slot->occupied = 1;
  if (!jitter->first_arrival_ns) {
    jitter->first_arrival_ns = packet->arrival_time_ns;
  }
  return MEZON_OK;
}

int mezon_audio_jitter_ready(mezon_audio_jitter_t *jitter, uint64_t now_ns) {
  uint64_t target_ns;
  if (!jitter || !jitter->initialized) {
    return 0;
  }
  if (jitter->started) {
    return 1;
  }
  target_ns = jitter->target_frames * MEZON_OPUS_FRAME_MS * 1000000ULL;
  if (jitter->count >= jitter->target_frames ||
      (jitter->first_arrival_ns && now_ns >= jitter->first_arrival_ns &&
       now_ns - jitter->first_arrival_ns >= target_ns)) {
    jitter->started = 1;
  }
  return jitter->started;
}

int mezon_audio_jitter_copy(mezon_audio_jitter_t *jitter, uint16_t seq,
                            uint8_t *payload, size_t *payload_len,
                            uint32_t *timestamp, uint8_t *marker, int remove) {
  mezon_audio_jitter_slot_t *slot;
  if (!jitter || !payload || !payload_len || !timestamp || !marker) {
    return 0;
  }
  slot = &jitter->slots[seq % MEZON_AUDIO_JITTER_SLOTS];
  if (!slot->occupied || slot->seq != seq) {
    return 0;
  }
  memcpy(payload, slot->payload, slot->len);
  *payload_len = slot->len;
  *timestamp = slot->timestamp;
  *marker = slot->marker;
  if (remove) {
    slot->occupied = 0;
    jitter->count--;
  }
  return 1;
}

void mezon_audio_jitter_advance(mezon_audio_jitter_t *jitter) {
  if (!jitter || !jitter->initialized) {
    return;
  }
  jitter->expected_seq++;
  jitter->expected_timestamp += MEZON_OPUS_FRAME_SAMPLES;
}
