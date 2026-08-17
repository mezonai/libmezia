#ifndef MEZIA_PEER_H
#define MEZIA_PEER_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_peer mezon_peer_t;

typedef void (*mezon_packet_callback_t)(const mezon_packet_t *packet,
                                        void *user_data);
typedef void (*mezon_error_callback_t)(mezon_status_t status, void *user_data);

typedef struct {
  const char *local_ip;
  uint16_t local_port;
  const char *remote_ip;
  uint16_t remote_port;
  size_t mtu;
  int receive_buffer_bytes;
  int send_buffer_bytes;
  mezon_packet_callback_t on_packet;
  mezon_error_callback_t on_error;
  void *user_data;
} mezon_peer_config_t;

mezon_peer_t *mezon_peer_create(const mezon_peer_config_t *config);
void mezon_peer_destroy(mezon_peer_t *peer);
mezon_status_t mezon_peer_start(mezon_peer_t *peer);
mezon_status_t mezon_peer_stop(mezon_peer_t *peer);
mezon_status_t mezon_peer_send(mezon_peer_t *peer, const mezon_packet_t *packet);
uint16_t mezon_peer_local_port(const mezon_peer_t *peer);
void mezon_peer_get_stats(const mezon_peer_t *peer, mezon_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
