#ifndef MEZIA_RTCP_H
#define MEZIA_RTCP_H

#include "mezia/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEZIA_RTCP_SR 200U
#define MEZIA_RTCP_RR 201U
#define MEZIA_RTCP_RTPFB 205U
#define MEZIA_RTCP_PSFB 206U
#define MEZIA_RTCP_FMT_NACK 1U
#define MEZIA_RTCP_FMT_PLI 1U
#define MEZIA_RTCP_MAX_NACK_SEQS 64U

typedef enum {
  MEZIA_RTCP_KIND_UNKNOWN = 0,
  MEZIA_RTCP_KIND_SR,
  MEZIA_RTCP_KIND_RR,
  MEZIA_RTCP_KIND_NACK,
  MEZIA_RTCP_KIND_PLI
} mezia_rtcp_kind_t;

typedef struct {
  uint32_t sender_ssrc;
  uint32_t ntp_msw;
  uint32_t ntp_lsw;
  uint32_t rtp_timestamp;
  uint32_t packet_count;
  uint32_t octet_count;
} mezia_rtcp_sr_t;

typedef struct {
  uint32_t sender_ssrc;
  uint32_t media_ssrc;
  uint8_t fraction_lost;
  uint32_t cumulative_lost;
  uint32_t extended_seq;
  uint32_t jitter;
  uint32_t last_sr;
  uint32_t delay_since_last_sr;
} mezia_rtcp_rr_t;

typedef struct {
  uint32_t sender_ssrc;
  uint32_t media_ssrc;
  uint16_t seqs[MEZIA_RTCP_MAX_NACK_SEQS];
  size_t seq_count;
} mezia_rtcp_nack_t;

typedef struct {
  uint32_t sender_ssrc;
  uint32_t media_ssrc;
} mezia_rtcp_pli_t;

typedef struct {
  mezia_rtcp_kind_t kind;
  mezia_rtcp_sr_t sr;
  mezia_rtcp_rr_t rr;
  mezia_rtcp_nack_t nack;
  mezia_rtcp_pli_t pli;
} mezia_rtcp_packet_t;

mezon_status_t mezia_rtcp_write_sr(const mezia_rtcp_sr_t *sr, uint8_t *out,
                                   size_t out_capacity, size_t *out_len);
mezon_status_t mezia_rtcp_write_rr(const mezia_rtcp_rr_t *rr, uint8_t *out,
                                   size_t out_capacity, size_t *out_len);
mezon_status_t mezia_rtcp_write_nack(const mezia_rtcp_nack_t *nack, uint8_t *out,
                                     size_t out_capacity, size_t *out_len);
mezon_status_t mezia_rtcp_write_pli(const mezia_rtcp_pli_t *pli, uint8_t *out,
                                    size_t out_capacity, size_t *out_len);
mezon_status_t mezia_rtcp_parse(const uint8_t *data, size_t len,
                                mezia_rtcp_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
