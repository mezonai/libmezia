#include "mezia_internal.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

static void write_u16(uint8_t *p, uint16_t value) {
  p[0] = (uint8_t)(value >> 8);
  p[1] = (uint8_t)value;
}

static void write_u32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)(value >> 24);
  p[1] = (uint8_t)(value >> 16);
  p[2] = (uint8_t)(value >> 8);
  p[3] = (uint8_t)value;
}

int mezon_seq_before(uint16_t lhs, uint16_t rhs) {
  return (int16_t)(lhs - rhs) < 0;
}

mezon_status_t mezon_rtp_serialize(const mezon_packet_t *packet, uint8_t *out,
                                   size_t out_capacity, size_t *out_len) {
  size_t total;
  if (!packet || !out || !out_len || (!packet->data && packet->len)) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (packet->payload_type > 127U) {
    return MEZON_ERR_INVALID_ARG;
  }
  total = MEZON_RTP_HEADER_SIZE + packet->len;
  if (out_capacity < total) {
    *out_len = total;
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  out[0] = 0x80U;
  out[1] = (uint8_t)((packet->marker ? 0x80U : 0U) | packet->payload_type);
  write_u16(out + 2, packet->seq);
  write_u32(out + 4, packet->timestamp);
  write_u32(out + 8, packet->ssrc);
  if (packet->len) {
    memcpy(out + MEZON_RTP_HEADER_SIZE, packet->data, packet->len);
  }
  *out_len = total;
  return MEZON_OK;
}

mezon_status_t mezon_rtp_parse(uint8_t *datagram, size_t datagram_len,
                               mezon_packet_t *packet) {
  size_t offset;
  unsigned int csrc_count;
  int has_extension;
  int has_padding;
  if (!datagram || !packet || datagram_len < MEZON_RTP_HEADER_SIZE) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  if ((datagram[0] >> 6) != 2U) {
    return MEZON_ERR_UNSUPPORTED;
  }
  has_padding = (datagram[0] & 0x20U) != 0U;
  has_extension = (datagram[0] & 0x10U) != 0U;
  csrc_count = datagram[0] & 0x0fU;
  offset = MEZON_RTP_HEADER_SIZE + (size_t)csrc_count * 4U;
  if (datagram_len < offset) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  if (has_extension) {
    uint16_t ext_words;
    if (datagram_len < offset + 4U) {
      return MEZON_ERR_MALFORMED_PACKET;
    }
    ext_words = read_u16(datagram + offset + 2);
    offset += 4U + (size_t)ext_words * 4U;
    if (datagram_len < offset) {
      return MEZON_ERR_MALFORMED_PACKET;
    }
  }
  if (has_padding) {
    uint8_t pad = datagram[datagram_len - 1U];
    if (pad == 0U || datagram_len - offset < pad) {
      return MEZON_ERR_MALFORMED_PACKET;
    }
    datagram_len -= pad;
  }
  memset(packet, 0, sizeof(*packet));
  packet->marker = (uint8_t)((datagram[1] >> 7) & 1U);
  packet->payload_type = (uint8_t)(datagram[1] & 0x7fU);
  packet->seq = read_u16(datagram + 2);
  packet->timestamp = read_u32(datagram + 4);
  packet->ssrc = read_u32(datagram + 8);
  packet->data = datagram + offset;
  packet->len = datagram_len - offset;
  packet->capacity = packet->len;
  return MEZON_OK;
}
