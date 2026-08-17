#ifndef mezia_TYPES_H
#define mezia_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MEZON_OK = 0,
  MEZON_ERR_INVALID_ARG = -1,
  MEZON_ERR_NOMEM = -2,
  MEZON_ERR_INTERNAL = -3,
  MEZON_ERR_NOT_READY = -4,
} mezon_status_t;

typedef enum {
  MEZON_CODEC_OPUS = 0,
  MEZON_CODEC_H264 = 1,
} mezon_codec_t;

typedef struct {
  uint8_t *data;
  size_t len;
  uint32_t timestamp;
  uint16_t seq;
  uint32_t ssrc;
} mezon_packet_t;

#ifdef __cplusplus
}
#endif

#endif
