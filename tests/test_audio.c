#include "mezia/audio.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                          \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #condition);                                                       \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

static int callback_count;
static size_t callback_samples;
static int64_t callback_energy;
static int callback_discontinuity;

static void on_audio(const int16_t *pcm, size_t samples_per_channel,
                     uint32_t timestamp, int discontinuity, void *user_data) {
  size_t i;
  (void)timestamp;
  (void)user_data;
  callback_count++;
  callback_samples = samples_per_channel;
  callback_discontinuity = discontinuity;
  callback_energy = 0;
  for (i = 0; i < samples_per_channel; ++i) {
    int32_t sample = pcm[i];
    callback_energy += sample < 0 ? -sample : sample;
  }
}

static void make_voice_frame(int16_t *pcm, int phase) {
  size_t i;
  for (i = 0; i < MEZON_OPUS_FRAME_SAMPLES; ++i) {
    int value = (int)((i + (size_t)phase * 37U) % 160U) - 80;
    pcm[i] = (int16_t)(value * 180);
  }
}

int main(void) {
  mezon_audio_config_t config;
  mezon_audio_ctx_t *sender;
  mezon_audio_ctx_t *receiver;
  int16_t pcm[MEZON_OPUS_FRAME_SAMPLES];
  uint8_t storage[4][300];
  mezon_packet_t packets[4];
  mezon_stats_t stats;
  uint64_t now_ns = 1000000000ULL;
  size_t count;
  size_t i;
  mezon_status_t status;

  memset(&config, 0, sizeof(config));
  config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  config.ssrc = 7;
  config.mtu = 300;
  config.jitter_target_ms = 40;
  config.jitter_max_ms = 120;
  sender = mezon_audio_create(&config);
  config.on_audio = on_audio;
  receiver = mezon_audio_create(&config);
  CHECK(sender && receiver);
  CHECK(mezon_audio_max_packets(sender, MEZON_OPUS_FRAME_SAMPLES) == 1);
  CHECK(mezon_audio_max_packets(sender, MEZON_OPUS_FRAME_SAMPLES - 1U) == 0);

  memset(packets, 0, sizeof(packets));
  for (i = 0; i < 4; ++i) {
    packets[i].data = storage[i];
    packets[i].capacity = sizeof(storage[i]);
    make_voice_frame(pcm, (int)i);
    count = 1;
    status = mezon_audio_packetize(sender, pcm, MEZON_OPUS_FRAME_SAMPLES,
                                   &packets[i], 1, &count);
    CHECK(status == MEZON_OK);
    CHECK(count == 1);
    CHECK(packets[i].len > 0 && packets[i].len < sizeof(pcm));
    CHECK(packets[i].seq == i);
    CHECK(packets[i].timestamp == i * MEZON_OPUS_FRAME_SAMPLES);
  }

  count = 0;
  make_voice_frame(pcm, 9);
  status = mezon_audio_packetize(sender, pcm, MEZON_OPUS_FRAME_SAMPLES,
                                 &packets[0], 0, &count);
  CHECK(status == MEZON_ERR_BUFFER_TOO_SMALL && count == 1);

  packets[0].arrival_time_ns = now_ns;
  packets[1].arrival_time_ns = now_ns + 10000000ULL;
  packets[3].arrival_time_ns = now_ns + 30000000ULL;
  CHECK(mezon_audio_receive(receiver, &packets[0]) == MEZON_OK);
  CHECK(mezon_audio_playout(receiver, now_ns) == MEZON_ERR_NOT_READY);
  CHECK(mezon_audio_receive(receiver, &packets[1]) == MEZON_OK);
  CHECK(mezon_audio_receive(receiver, &packets[3]) == MEZON_OK);

  CHECK(mezon_audio_playout(receiver, now_ns + 40000000ULL) == MEZON_OK);
  CHECK(callback_count == 1);
  CHECK(callback_samples == MEZON_OPUS_FRAME_SAMPLES);
  CHECK(callback_energy > 0);
  CHECK(callback_discontinuity == 0);

  CHECK(mezon_audio_playout(receiver, now_ns + 60000000ULL) == MEZON_OK);
  CHECK(callback_count == 2);
  CHECK(mezon_audio_playout(receiver, now_ns + 80000000ULL) == MEZON_OK);
  CHECK(callback_count == 3);

  memset(&stats, 0, sizeof(stats));
  mezon_audio_get_stats(receiver, &stats);
  CHECK(stats.audio_frames_decoded == 3);
  CHECK(stats.audio_frames_fec_recovered == 1);
  CHECK(stats.sequence_gaps == 1);

  CHECK(mezon_audio_receive(receiver, &packets[0]) == MEZON_OK);
  mezon_audio_get_stats(receiver, &stats);
  CHECK(stats.late_packets >= 1);

  mezon_audio_destroy(receiver);
  mezon_audio_destroy(sender);
  return 0;
}
