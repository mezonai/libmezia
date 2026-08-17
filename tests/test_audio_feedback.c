#include "audio_feedback.h"

#include <assert.h>
#include <string.h>

int main(void) {
  mezon_audio_feedback_t input = {0x1234, 0xabcdef01U, 50, 47};
  mezon_audio_feedback_t output;
  uint8_t wire[MEZON_AUDIO_FEEDBACK_SIZE];
  assert(mezon_audio_feedback_serialize(&input, wire, sizeof(wire)) == MEZON_OK);
  assert(wire[0] == 0x4d && wire[1] == 1);
  assert(wire[2] == 0x12 && wire[3] == 0x34);
  memset(&output, 0, sizeof(output));
  assert(mezon_audio_feedback_parse(wire, sizeof(wire), &output) == MEZON_OK);
  assert(output.report_seq == input.report_seq);
  assert(output.media_ssrc == input.media_ssrc);
  assert(output.expected == 50 && output.received == 47);
  wire[1] = 2;
  assert(mezon_audio_feedback_parse(wire, sizeof(wire), &output) ==
         MEZON_ERR_UNSUPPORTED);
  assert(mezon_audio_feedback_parse(wire, sizeof(wire) - 1U, &output) ==
         MEZON_ERR_MALFORMED_PACKET);
  return 0;
}
