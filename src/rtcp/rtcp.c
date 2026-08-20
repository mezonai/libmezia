#include "mezia/rtcp.h"

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

static mezon_status_t need(size_t cap, size_t want, size_t *out_len) {
  if (cap < want) {
    if (out_len) {
      *out_len = want;
    }
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  return MEZON_OK;
}

mezon_status_t mezia_rtcp_write_sr(const mezia_rtcp_sr_t *sr, uint8_t *out,
                                   size_t out_capacity, size_t *out_len) {
  mezon_status_t status;
  if (!sr || !out || !out_len) {
    return MEZON_ERR_INVALID_ARG;
  }
  status = need(out_capacity, 28, out_len);
  if (status != MEZON_OK) {
    return status;
  }
  out[0] = 0x80U;
  out[1] = MEZIA_RTCP_SR;
  write_u16(out + 2, 6);
  write_u32(out + 4, sr->sender_ssrc);
  write_u32(out + 8, sr->ntp_msw);
  write_u32(out + 12, sr->ntp_lsw);
  write_u32(out + 16, sr->rtp_timestamp);
  write_u32(out + 20, sr->packet_count);
  write_u32(out + 24, sr->octet_count);
  *out_len = 28;
  return MEZON_OK;
}

mezon_status_t mezia_rtcp_write_rr(const mezia_rtcp_rr_t *rr, uint8_t *out,
                                   size_t out_capacity, size_t *out_len) {
  mezon_status_t status;
  if (!rr || !out || !out_len) {
    return MEZON_ERR_INVALID_ARG;
  }
  status = need(out_capacity, 32, out_len);
  if (status != MEZON_OK) {
    return status;
  }
  out[0] = 0x81U;
  out[1] = MEZIA_RTCP_RR;
  write_u16(out + 2, 7);
  write_u32(out + 4, rr->sender_ssrc);
  write_u32(out + 8, rr->media_ssrc);
  out[12] = rr->fraction_lost;
  out[13] = (uint8_t)((rr->cumulative_lost >> 16) & 0xffU);
  out[14] = (uint8_t)((rr->cumulative_lost >> 8) & 0xffU);
  out[15] = (uint8_t)(rr->cumulative_lost & 0xffU);
  write_u32(out + 16, rr->extended_seq);
  write_u32(out + 20, rr->jitter);
  write_u32(out + 24, rr->last_sr);
  write_u32(out + 28, rr->delay_since_last_sr);
  *out_len = 32;
  return MEZON_OK;
}

mezon_status_t mezia_rtcp_write_nack(const mezia_rtcp_nack_t *nack, uint8_t *out,
                                     size_t out_capacity, size_t *out_len) {
  size_t i;
  uint16_t pid;
  uint16_t blp = 0;
  mezon_status_t status;
  if (!nack || !out || !out_len || nack->seq_count == 0 ||
      nack->seq_count > MEZIA_RTCP_MAX_NACK_SEQS) {
    return MEZON_ERR_INVALID_ARG;
  }
  status = need(out_capacity, 16, out_len);
  if (status != MEZON_OK) {
    return status;
  }
  pid = nack->seqs[0];
  for (i = 1; i < nack->seq_count; i++) {
    uint16_t delta = (uint16_t)(nack->seqs[i] - pid);
    if (delta == 0 || delta > 16U) {
      return MEZON_ERR_UNSUPPORTED;
    }
    blp |= (uint16_t)(1U << (delta - 1U));
  }
  out[0] = (uint8_t)(0x80U | MEZIA_RTCP_FMT_NACK);
  out[1] = MEZIA_RTCP_RTPFB;
  write_u16(out + 2, 3);
  write_u32(out + 4, nack->sender_ssrc);
  write_u32(out + 8, nack->media_ssrc);
  write_u16(out + 12, pid);
  write_u16(out + 14, blp);
  *out_len = 16;
  return MEZON_OK;
}

mezon_status_t mezia_rtcp_write_pli(const mezia_rtcp_pli_t *pli, uint8_t *out,
                                    size_t out_capacity, size_t *out_len) {
  mezon_status_t status;
  if (!pli || !out || !out_len) {
    return MEZON_ERR_INVALID_ARG;
  }
  status = need(out_capacity, 12, out_len);
  if (status != MEZON_OK) {
    return status;
  }
  out[0] = (uint8_t)(0x80U | MEZIA_RTCP_FMT_PLI);
  out[1] = MEZIA_RTCP_PSFB;
  write_u16(out + 2, 2);
  write_u32(out + 4, pli->sender_ssrc);
  write_u32(out + 8, pli->media_ssrc);
  *out_len = 12;
  return MEZON_OK;
}

static void parse_nack_fci(const uint8_t *fci, size_t fci_len,
                           mezia_rtcp_nack_t *nack) {
  size_t off = 0;
  nack->seq_count = 0;
  while (off + 4U <= fci_len && nack->seq_count < MEZIA_RTCP_MAX_NACK_SEQS) {
    uint16_t pid = read_u16(fci + off);
    uint16_t blp = read_u16(fci + off + 2);
    unsigned int bit;
    nack->seqs[nack->seq_count++] = pid;
    for (bit = 0; bit < 16U && nack->seq_count < MEZIA_RTCP_MAX_NACK_SEQS;
         bit++) {
      if (blp & (uint16_t)(1U << bit)) {
        nack->seqs[nack->seq_count++] = (uint16_t)(pid + bit + 1U);
      }
    }
    off += 4U;
  }
}

mezon_status_t mezia_rtcp_parse(const uint8_t *data, size_t len,
                                mezia_rtcp_packet_t *packet) {
  uint8_t pt;
  uint8_t fmt;
  uint16_t words;
  size_t packet_len;
  if (!data || !packet || len < 8U) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  if ((data[0] >> 6) != 2U) {
    return MEZON_ERR_UNSUPPORTED;
  }
  memset(packet, 0, sizeof(*packet));
  fmt = (uint8_t)(data[0] & 0x1fU);
  pt = data[1];
  words = read_u16(data + 2);
  packet_len = ((size_t)words + 1U) * 4U;
  if (packet_len > len || packet_len < 8U) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  if (pt == MEZIA_RTCP_SR && packet_len >= 28U) {
    packet->kind = MEZIA_RTCP_KIND_SR;
    packet->sr.sender_ssrc = read_u32(data + 4);
    packet->sr.ntp_msw = read_u32(data + 8);
    packet->sr.ntp_lsw = read_u32(data + 12);
    packet->sr.rtp_timestamp = read_u32(data + 16);
    packet->sr.packet_count = read_u32(data + 20);
    packet->sr.octet_count = read_u32(data + 24);
    return MEZON_OK;
  }
  if (pt == MEZIA_RTCP_RR && packet_len >= 32U) {
    packet->kind = MEZIA_RTCP_KIND_RR;
    packet->rr.sender_ssrc = read_u32(data + 4);
    packet->rr.media_ssrc = read_u32(data + 8);
    packet->rr.fraction_lost = data[12];
    packet->rr.cumulative_lost =
        ((uint32_t)data[13] << 16) | ((uint32_t)data[14] << 8) | data[15];
    packet->rr.extended_seq = read_u32(data + 16);
    packet->rr.jitter = read_u32(data + 20);
    packet->rr.last_sr = read_u32(data + 24);
    packet->rr.delay_since_last_sr = read_u32(data + 28);
    return MEZON_OK;
  }
  if (pt == MEZIA_RTCP_RTPFB && fmt == MEZIA_RTCP_FMT_NACK && packet_len >= 16U) {
    packet->kind = MEZIA_RTCP_KIND_NACK;
    packet->nack.sender_ssrc = read_u32(data + 4);
    packet->nack.media_ssrc = read_u32(data + 8);
    parse_nack_fci(data + 12, packet_len - 12U, &packet->nack);
    return MEZON_OK;
  }
  if (pt == MEZIA_RTCP_PSFB && fmt == MEZIA_RTCP_FMT_PLI && packet_len >= 12U) {
    packet->kind = MEZIA_RTCP_KIND_PLI;
    packet->pli.sender_ssrc = read_u32(data + 4);
    packet->pli.media_ssrc = read_u32(data + 8);
    return MEZON_OK;
  }
  packet->kind = MEZIA_RTCP_KIND_UNKNOWN;
  return MEZON_ERR_UNSUPPORTED;
}
