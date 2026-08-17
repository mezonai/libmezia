#include "mezia/peer.h"

struct mezon_peer {
  char remote_ip[64];
  uint16_t remote_port;
};

mezon_peer_t *mezon_peer_create(const char *remote_ip, uint16_t remote_port) {
  (void)remote_ip;
  (void)remote_port;
  return NULL;
}

void mezon_peer_destroy(mezon_peer_t *peer) { (void)peer; }

mezon_status_t mezon_peer_connect(mezon_peer_t *peer) {
  (void)peer;
  return MEZON_ERR_NOT_READY;
}

mezon_status_t mezon_peer_send(mezon_peer_t *peer, const mezon_packet_t *pkt) {
  (void)peer;
  (void)pkt;
  return MEZON_ERR_NOT_READY;
}
