#include "mezia/video.h"

#include <assert.h>
#include <string.h>

static uint8_t received[256];
static size_t received_len;
static int callback_count;

static void on_nal(const uint8_t *nal, size_t nal_len, uint32_t timestamp,
                   int end_of_access_unit, int discontinuity, void *user_data) {
  (void)timestamp;
  (void)end_of_access_unit;
  (void)discontinuity;
  (void)user_data;
  memcpy(received, nal, nal_len);
  received_len = nal_len;
  callback_count++;
}

int main(void) {
  mezon_video_config_t config;
  mezon_video_ctx_t *sender;
  mezon_video_ctx_t *receiver;
  uint8_t nal[100];
  uint8_t storage[6][32];
  mezon_packet_t packets[6];
  size_t count = 0;
  size_t i;
  memset(&config, 0, sizeof(config));
  config.payload_type = 97;
  config.ssrc = 9;
  config.mtu = 32;
  config.max_nal_size = sizeof(received);
  sender = mezon_video_create(&config);
  config.on_nal = on_nal;
  receiver = mezon_video_create(&config);
  assert(sender && receiver);
  nal[0] = 0x65;
  for (i = 1; i < sizeof(nal); ++i) {
    nal[i] = (uint8_t)i;
  }
  memset(packets, 0, sizeof(packets));
  for (i = 0; i < 6; ++i) {
    packets[i].data = storage[i];
    packets[i].capacity = sizeof(storage[i]);
  }
  assert(mezon_video_max_packets_for_nal(sender, sizeof(nal)) == 6);
  assert(mezon_video_packetize_nal(sender, nal, sizeof(nal), 9000, 1,
                                   packets, 6, &count) == MEZON_OK);
  assert(count == 6);
  for (i = 0; i < count; ++i) {
    assert(mezon_video_receive(receiver, &packets[i]) == MEZON_OK);
  }
  assert(callback_count == 1);
  assert(received_len == sizeof(nal));
  assert(memcmp(received, nal, sizeof(nal)) == 0);
  mezon_video_destroy(receiver);
  mezon_video_destroy(sender);
  return 0;
}
