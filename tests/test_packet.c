#include "mezia_internal.h"

#include <assert.h>
#include <string.h>

int main(void) {
  uint8_t payload[] = {1, 2, 3};
  uint8_t wire[32];
  mezon_packet_t input;
  mezon_packet_t output;
  size_t wire_len = 0;
  memset(&input, 0, sizeof(input));
  input.data = payload;
  input.len = sizeof(payload);
  input.payload_type = 97;
  input.marker = 1;
  input.seq = 65535;
  input.timestamp = 0x12345678U;
  input.ssrc = 0xabcdef01U;
  assert(mezon_rtp_serialize(&input, wire, sizeof(wire), &wire_len) == MEZON_OK);
  assert(wire_len == 15);
  assert(mezon_rtp_parse(wire, wire_len, &output) == MEZON_OK);
  assert(output.payload_type == input.payload_type);
  assert(output.marker == 1);
  assert(output.seq == input.seq);
  assert(output.timestamp == input.timestamp);
  assert(output.ssrc == input.ssrc);
  assert(output.len == sizeof(payload));
  assert(memcmp(output.data, payload, sizeof(payload)) == 0);
  wire[0] = 0;
  assert(mezon_rtp_parse(wire, wire_len, &output) == MEZON_ERR_UNSUPPORTED);
  assert(mezon_seq_before(65535, 0));
  return 0;
}
