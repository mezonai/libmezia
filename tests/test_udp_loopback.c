#include "mezia/peer.h"

#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(value) Sleep(value)
#else
#include <time.h>
static void sleep_ms(long value) {
  struct timespec ts = {value / 1000, (value % 1000) * 1000000L};
  nanosleep(&ts, NULL);
}
#endif

static volatile int received;

static void on_packet(const mezon_packet_t *packet, void *user_data) {
  (void)user_data;
  if (packet->len == 3 && packet->data[0] == 1) {
    received = 1;
  }
}

int main(void) {
  mezon_peer_config_t a_config;
  mezon_peer_config_t b_config;
  mezon_peer_t *a;
  mezon_peer_t *b;
  uint8_t payload[] = {1, 2, 3};
  mezon_packet_t packet;
  int attempts;
  memset(&a_config, 0, sizeof(a_config));
  memset(&b_config, 0, sizeof(b_config));
  a_config.local_ip = "127.0.0.1";
  a_config.local_port = 39001;
  a_config.remote_ip = "127.0.0.1";
  a_config.remote_port = 39002;
  a_config.mtu = 1200;
  b_config.local_ip = "127.0.0.1";
  b_config.local_port = 39002;
  b_config.remote_ip = "127.0.0.1";
  b_config.remote_port = 39001;
  b_config.mtu = 1200;
  b_config.on_packet = on_packet;
  a = mezon_peer_create(&a_config);
  b = mezon_peer_create(&b_config);
  assert(a && b);
  assert(mezon_peer_start(a) == MEZON_OK);
  assert(mezon_peer_start(b) == MEZON_OK);
  memset(&packet, 0, sizeof(packet));
  packet.data = payload;
  packet.len = sizeof(payload);
  packet.payload_type = 96;
  packet.seq = 1;
  packet.ssrc = 2;
  assert(mezon_peer_send(a, &packet) == MEZON_OK);
  for (attempts = 0; attempts < 50 && !received; ++attempts) {
    sleep_ms(10);
  }
  assert(received);
  assert(mezon_peer_stop(b) == MEZON_OK);
  assert(mezon_peer_stop(a) == MEZON_OK);
  mezon_peer_destroy(b);
  mezon_peer_destroy(a);
  return 0;
}
