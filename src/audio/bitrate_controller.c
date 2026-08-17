#include "bitrate_controller.h"

static uint32_t clamp_round(uint32_t value, uint32_t minimum,
                            uint32_t maximum) {
  value = ((value + 500U) / 1000U) * 1000U;
  if (value < minimum) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}

void mezon_bitrate_controller_init(mezon_bitrate_controller_t *controller,
                                   uint32_t minimum, uint32_t initial,
                                   uint32_t maximum) {
  controller->min_bitrate = minimum;
  controller->max_bitrate = maximum;
  controller->current_bitrate = initial;
  controller->loss_percent = MEZON_OPUS_EXPECTED_LOSS_PERCENT;
  controller->clean_reports = 0;
}

mezon_bitrate_decision_t mezon_bitrate_controller_update(
    mezon_bitrate_controller_t *controller, uint32_t expected,
    uint32_t received) {
  mezon_bitrate_decision_t decision = {0};
  uint32_t loss;
  uint32_t target;
  if (!controller) {
    return decision;
  }
  decision.bitrate = controller->current_bitrate;
  decision.loss_percent = controller->loss_percent;
  decision.increased = 0;
  decision.decreased = 0;
  if (!expected || received > expected) {
    return decision;
  }
  loss = ((expected - received) * 100U + expected - 1U) / expected;
  if (loss > 20U) {
    loss = 20U;
  }
  controller->loss_percent =
      (uint8_t)((controller->loss_percent * 3U + loss + 2U) / 4U);
  target = controller->current_bitrate;
  if (loss >= 10U) {
    target = controller->current_bitrate * 75U / 100U;
  } else if (loss >= 5U) {
    target = controller->current_bitrate * 85U / 100U;
  } else if (loss >= 2U) {
    target = controller->current_bitrate * 95U / 100U;
  } else if (expected >= 20U) {
    controller->clean_reports++;
    if (controller->clean_reports >= 3U) {
      target = controller->current_bitrate + 2000U;
      controller->clean_reports = 0;
    }
  }
  if (loss >= 2U) {
    controller->clean_reports = 0;
  }
  target = clamp_round(target, controller->min_bitrate,
                       controller->max_bitrate);
  decision.increased = target > controller->current_bitrate;
  decision.decreased = target < controller->current_bitrate;
  controller->current_bitrate = target;
  decision.bitrate = target;
  decision.loss_percent = controller->loss_percent;
  return decision;
}
