#ifndef MEZIA_INTERNAL_H
#define MEZIA_INTERNAL_H

#include "mezia/types.h"

#include <stddef.h>
#include <stdint.h>

uint64_t mezon_clock_now_ns(void);
int mezon_seq_before(uint16_t lhs, uint16_t rhs);
mezon_status_t mezon_rtp_serialize(const mezon_packet_t *packet, uint8_t *out,
                                   size_t out_capacity, size_t *out_len);
mezon_status_t mezon_rtp_parse(uint8_t *datagram, size_t datagram_len,
                               mezon_packet_t *packet);

#endif
