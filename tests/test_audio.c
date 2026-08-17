#include "mezia/audio.h"

#include <assert.h>
#include <string.h>

static int callback_count;
static int16_t callback_pcm[8];
static size_t callback_samples;

static void on_audio(const int16_t *pcm, size_t samples_per_channel,
                     uint32_t timestamp, int discontinuity, void *user_data) {
  (void)timestamp;
  (void)discontinuity;
  (void)user_data;
  callback_count++;
  callback_samples = samples_per_channel;
  memcpy(callback_pcm, pcm, samples_per_channel * 2U * sizeof(int16_t));
}

int main(void) {
  mezon_audio_config_t config;
  mezon_audio_ctx_t *sender;
  mezon_audio_ctx_t *receiver;
  int16_t pcm[] = {1, -2, 300, -400};
  uint8_t storage[64];
  mezon_packet_t packet;
  size_t count = 0;
  memset(&config, 0, sizeof(config));
  config.sample_rate = 48000;
  config.channels = 2;
  config.payload_type = 96;
  config.ssrc = 7;
  config.mtu = 64;
  sender = mezon_audio_create(&config);
  config.on_audio = on_audio;
  receiver = mezon_audio_create(&config);
  assert(sender && receiver);
  memset(&packet, 0, sizeof(packet));
  packet.data = storage;
  packet.capacity = sizeof(storage);
  assert(mezon_audio_packetize(sender, pcm, 2, &packet, 1, &count) == MEZON_OK);
  assert(count == 1);
  assert(packet.len == sizeof(pcm));
  assert(storage[0] == 0 && storage[1] == 1);
  assert(mezon_audio_receive(receiver, &packet) == MEZON_OK);
  assert(callback_count == 1);
  assert(callback_samples == 2);
  assert(memcmp(callback_pcm, pcm, sizeof(pcm)) == 0);
  mezon_audio_destroy(receiver);
  mezon_audio_destroy(sender);
  return 0;
}
