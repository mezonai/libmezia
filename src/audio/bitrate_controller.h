#ifndef MEZIA_BITRATE_CONTROLLER_H
#define MEZIA_BITRATE_CONTROLLER_H

#include "mezia/types.h"

#include <stdint.h>

typedef struct {
  uint32_t min_bitrate;
  uint32_t max_bitrate;
  uint32_t current_bitrate;
  uint8_t loss_percent;
  uint8_t clean_reports;
} mezon_bitrate_controller_t;

typedef struct {
  uint32_t bitrate;
  uint8_t loss_percent;
  int increased;
  int decreased;
} mezon_bitrate_decision_t;

void mezon_bitrate_controller_init(mezon_bitrate_controller_t *controller,
                                   uint32_t minimum, uint32_t initial,
                                   uint32_t maximum);
mezon_bitrate_decision_t mezon_bitrate_controller_update(
    mezon_bitrate_controller_t *controller, uint32_t expected,
    uint32_t received);

#endif
