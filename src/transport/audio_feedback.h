#ifndef MEZIA_AUDIO_FEEDBACK_H
#define MEZIA_AUDIO_FEEDBACK_H

#include "mezia/types.h"

#define MEZON_AUDIO_FEEDBACK_SIZE 16U

typedef struct {
  uint16_t report_seq;
  uint32_t media_ssrc;
  uint32_t expected;
  uint32_t received;
} mezon_audio_feedback_t;

mezon_status_t mezon_audio_feedback_serialize(
    const mezon_audio_feedback_t *feedback, uint8_t *output, size_t capacity);
mezon_status_t mezon_audio_feedback_parse(const uint8_t *payload, size_t len,
                                          mezon_audio_feedback_t *feedback);

#endif
