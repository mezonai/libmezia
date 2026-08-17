#include "audio_internal.h"
#include "mezia/audio.h"
#include "mezia/media.h"
#include "mezia/types.h"

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

static void make_frame(int16_t *pcm, unsigned phase) {
  size_t i;
  for (i = 0; i < MEZON_OPUS_FRAME_SAMPLES; ++i) {
    int value = (int)((i + phase * 29U) % 160U) - 80;
    pcm[i] = (int16_t)(value * 180);
  }
}

static int encode_frame(mezon_audio_ctx_t *audio, mezon_packet_t *packet,
                        int16_t *pcm, unsigned phase) {
  size_t count = 1;
  make_frame(pcm, phase);
  return mezon_audio_packetize(audio, pcm, MEZON_OPUS_FRAME_SAMPLES, packet, 1,
                               &count) == MEZON_OK &&
         count == 1;
}

int main(void) {
  mezon_audio_config_t config;
  mezon_audio_ctx_t *audio;
  mezon_audio_feedback_t feedback;
  mezon_packet_t packet;
  mezon_stats_t stats;
  int16_t pcm[MEZON_OPUS_FRAME_SAMPLES];
  uint8_t payload[300];
  unsigned i;

  memset(&config, 0, sizeof(config));
  config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  config.ssrc = 0x12345678U;
  config.mtu = sizeof(payload);
  config.jitter_target_ms = 40;
  config.jitter_max_ms = 120;
  config.adaptation.enabled = 1;
  config.adaptation.min_bitrate_bps = 16000;
  config.adaptation.initial_bitrate_bps = 24000;
  config.adaptation.max_bitrate_bps = 48000;
  config.adaptation.report_interval_ms = 200;
  config.adaptation.feedback_timeout_ms = 400;
  audio = mezon_audio_create(&config);
  CHECK(audio != NULL);

  memset(&packet, 0, sizeof(packet));
  packet.data = payload;
  packet.capacity = sizeof(payload);
  CHECK(encode_frame(audio, &packet, pcm, 0));
  memset(&stats, 0, sizeof(stats));
  mezon_audio_get_stats(audio, &stats);
  CHECK(stats.audio_current_bitrate_bps == 24000);

  memset(&feedback, 0, sizeof(feedback));
  feedback.media_ssrc = config.ssrc;
  feedback.expected = 50;
  feedback.received = 50;
  for (i = 0; i < 3; ++i) {
    feedback.report_seq = (uint16_t)i;
    CHECK(mezon_audio_submit_receiver_report(audio, &feedback,
                                             1000000000ULL + i) == MEZON_OK);
    CHECK(encode_frame(audio, &packet, pcm, i + 1U));
  }
  mezon_audio_get_stats(audio, &stats);
  CHECK(stats.audio_current_bitrate_bps == 26000);
  CHECK(stats.audio_bitrate_increases == 1);

  feedback.report_seq = 3;
  feedback.received = 45;
  CHECK(mezon_audio_submit_receiver_report(audio, &feedback, 1000000100ULL) ==
        MEZON_OK);
  CHECK(encode_frame(audio, &packet, pcm, 5));
  mezon_audio_get_stats(audio, &stats);
  CHECK(stats.audio_current_bitrate_bps == 20000);
  CHECK(stats.audio_bitrate_decreases == 1);

  CHECK(mezon_audio_submit_receiver_report(audio, &feedback, 1000000200ULL) ==
        MEZON_ERR_STATE);
  mezon_audio_get_stats(audio, &stats);
  CHECK(stats.audio_adaptation_reports_rejected == 1);

  feedback.report_seq = 4;
  feedback.received = 50;
  CHECK(mezon_audio_submit_receiver_report(audio, &feedback, 1) == MEZON_OK);
  CHECK(encode_frame(audio, &packet, pcm, 6));
  CHECK(encode_frame(audio, &packet, pcm, 7));
  mezon_audio_get_stats(audio, &stats);
  CHECK(stats.audio_current_bitrate_bps == 16000);
  CHECK(stats.audio_adaptation_stale_events == 1);
  CHECK(stats.audio_bitrate_decreases == 2);

  mezon_audio_destroy(audio);

  {
    mezia_config_t media_config;
    mezon_audio_config_t audio_config;
    memset(&media_config, 0, sizeof(media_config));
    memset(&audio_config, 0, sizeof(audio_config));
    audio_config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
    audio_config.ssrc = 99;
    audio_config.mtu = 300;
    audio_config.adaptation.enabled = 1;
    media_config.audio = &audio_config;
    media_config.control_payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
    media_config.peer.local_ip = "127.0.0.1";
    media_config.peer.remote_ip = "127.0.0.1";
    media_config.peer.remote_port = 39003;
    media_config.peer.mtu = 300;
    CHECK(mezia_create(&media_config) == NULL);
  }
  return 0;
}
