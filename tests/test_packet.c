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

  {
    uint8_t ext_wire[] = {
        0x90, 0x6f, 0x00, 0x07, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0xbe, 0xde, 0x00, 0x01, 0x10, 0x30, 0x00, 0x00, 0xaa, 0xbb};
    assert(mezon_rtp_parse(ext_wire, sizeof(ext_wire), &output) == MEZON_OK);
    assert(output.payload_type == 111);
    assert(output.seq == 7);
    assert(output.len == 2);
    assert(output.data[0] == 0xaa && output.data[1] == 0xbb);
  }

  assert(mezon_seq_before(65535, 0));
  return 0;
}
