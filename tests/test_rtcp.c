#include "mezia/rtcp.h"

#include <assert.h>
#include <string.h>

int main(void) {
  uint8_t wire[64];
  size_t len = 0;
  mezia_rtcp_packet_t parsed;
  mezia_rtcp_sr_t sr;
  mezia_rtcp_rr_t rr;
  mezia_rtcp_nack_t nack;
  mezia_rtcp_pli_t pli;

  memset(&sr, 0, sizeof(sr));
  sr.sender_ssrc = 0x11111111U;
  sr.rtp_timestamp = 48000;
  sr.packet_count = 10;
  sr.octet_count = 400;
  assert(mezia_rtcp_write_sr(&sr, wire, sizeof(wire), &len) == MEZON_OK);
  assert(mezia_rtcp_parse(wire, len, &parsed) == MEZON_OK);
  assert(parsed.kind == MEZIA_RTCP_KIND_SR);
  assert(parsed.sr.packet_count == 10);

  memset(&rr, 0, sizeof(rr));
  rr.sender_ssrc = 1;
  rr.media_ssrc = 2;
  rr.fraction_lost = 12;
  rr.cumulative_lost = 7;
  rr.extended_seq = 1000;
  rr.jitter = 5;
  assert(mezia_rtcp_write_rr(&rr, wire, sizeof(wire), &len) == MEZON_OK);
  assert(mezia_rtcp_parse(wire, len, &parsed) == MEZON_OK);
  assert(parsed.kind == MEZIA_RTCP_KIND_RR);
  assert(parsed.rr.fraction_lost == 12);
  assert(parsed.rr.cumulative_lost == 7);

  memset(&nack, 0, sizeof(nack));
  nack.sender_ssrc = 0xdeadbeefU;
  nack.media_ssrc = 0x11223344U;
  nack.seqs[0] = 100;
  nack.seqs[1] = 101;
  nack.seqs[2] = 103;
  nack.seq_count = 3;
  assert(mezia_rtcp_write_nack(&nack, wire, sizeof(wire), &len) == MEZON_OK);
  assert(mezia_rtcp_parse(wire, len, &parsed) == MEZON_OK);
  assert(parsed.kind == MEZIA_RTCP_KIND_NACK);
  assert(parsed.nack.seq_count == 3);
  assert(parsed.nack.seqs[0] == 100);
  assert(parsed.nack.seqs[1] == 101);
  assert(parsed.nack.seqs[2] == 103);

  memset(&pli, 0, sizeof(pli));
  pli.sender_ssrc = 9;
  pli.media_ssrc = 8;
  assert(mezia_rtcp_write_pli(&pli, wire, sizeof(wire), &len) == MEZON_OK);
  assert(len == 12);
  assert(mezia_rtcp_parse(wire, len, &parsed) == MEZON_OK);
  assert(parsed.kind == MEZIA_RTCP_KIND_PLI);
  assert(parsed.pli.media_ssrc == 8);
  return 0;
}
