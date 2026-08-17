#ifndef mezia_PEER_H
#define mezia_PEER_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_peer mezon_peer_t;

mezon_peer_t *mezon_peer_create(const char *remote_ip, uint16_t remote_port);
void mezon_peer_destroy(mezon_peer_t *peer);
mezon_status_t mezon_peer_connect(mezon_peer_t *peer);
mezon_status_t mezon_peer_send(mezon_peer_t *peer, const mezon_packet_t *pkt);

#ifdef __cplusplus
}
#endif

#endif
