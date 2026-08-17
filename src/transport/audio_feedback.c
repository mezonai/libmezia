#include "audio_feedback.h"

#define FEEDBACK_MAGIC 0x4dU
#define FEEDBACK_VERSION 1U

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

static uint16_t read_u16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}

mezon_status_t mezon_audio_feedback_serialize(
    const mezon_audio_feedback_t *feedback, uint8_t *output, size_t capacity) {
  if (!feedback || !output || capacity < MEZON_AUDIO_FEEDBACK_SIZE ||
      !feedback->expected || feedback->received > feedback->expected) {
    return MEZON_ERR_INVALID_ARG;
  }
  output[0] = FEEDBACK_MAGIC;
  output[1] = FEEDBACK_VERSION;
  write_u16(output + 2, feedback->report_seq);
  write_u32(output + 4, feedback->media_ssrc);
  write_u32(output + 8, feedback->expected);
  write_u32(output + 12, feedback->received);
  return MEZON_OK;
}

mezon_status_t mezon_audio_feedback_parse(const uint8_t *payload, size_t len,
                                          mezon_audio_feedback_t *feedback) {
  if (!payload || !feedback || len != MEZON_AUDIO_FEEDBACK_SIZE) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  if (payload[0] != FEEDBACK_MAGIC || payload[1] != FEEDBACK_VERSION) {
    return MEZON_ERR_UNSUPPORTED;
  }
  feedback->report_seq = read_u16(payload + 2);
  feedback->media_ssrc = read_u32(payload + 4);
  feedback->expected = read_u32(payload + 8);
  feedback->received = read_u32(payload + 12);
  if (!feedback->expected || feedback->received > feedback->expected) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  return MEZON_OK;
}
