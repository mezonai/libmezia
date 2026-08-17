#include "bitrate_controller.h"

#include <assert.h>

int main(void) {
  mezon_bitrate_controller_t c;
  mezon_bitrate_decision_t d;
  mezon_bitrate_controller_init(&c, 16000, 24000, 48000);
  d = mezon_bitrate_controller_update(&c, 50, 50);
  assert(d.bitrate == 24000 && !d.increased);
  mezon_bitrate_controller_update(&c, 50, 50);
  d = mezon_bitrate_controller_update(&c, 50, 50);
  assert(d.bitrate == 26000 && d.increased);
  d = mezon_bitrate_controller_update(&c, 50, 45);
  assert(d.bitrate == 20000 && d.decreased);
  d = mezon_bitrate_controller_update(&c, 50, 40);
  assert(d.bitrate == 16000 && d.decreased);
  d = mezon_bitrate_controller_update(&c, 50, 40);
  assert(d.bitrate == 16000);
  return 0;
}
