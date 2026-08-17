#ifndef MEZIA_TYPES_H
#define MEZIA_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEZON_RTP_HEADER_SIZE 12U
#define MEZON_DEFAULT_MTU 1200U
#define MEZON_MAX_DATAGRAM_SIZE 1500U
#define MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE 96U
#define MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE 97U

typedef enum {
  MEZON_OK = 0,
  MEZON_ERR_INVALID_ARG = -1,
  MEZON_ERR_NOMEM = -2,
  MEZON_ERR_INTERNAL = -3,
  MEZON_ERR_NOT_READY = -4,
  MEZON_ERR_BUFFER_TOO_SMALL = -5,
  MEZON_ERR_MALFORMED_PACKET = -6,
  MEZON_ERR_NETWORK = -7,
  MEZON_ERR_WOULD_BLOCK = -8,
  MEZON_ERR_UNSUPPORTED = -9,
  MEZON_ERR_STATE = -10,
} mezon_status_t;

typedef enum {
  MEZON_MEDIA_AUDIO_PCM_L16 = 0,
  MEZON_MEDIA_VIDEO_H264 = 1,
} mezon_media_type_t;

/* Packet bytes are always caller-owned. The library never retains data. */
typedef struct {
  uint8_t *data;
  size_t len;
  size_t capacity;
  uint32_t timestamp;
  uint16_t seq;
  uint32_t ssrc;
  uint64_t arrival_time_ns;
  uint8_t payload_type;
  uint8_t marker;
} mezon_packet_t;

typedef struct {
  uint64_t packets_sent;
  uint64_t packets_received;
  uint64_t bytes_sent;
  uint64_t bytes_received;
  uint64_t malformed_packets;
  uint64_t unknown_payloads;
  uint64_t sequence_gaps;
  uint64_t duplicate_packets;
  uint64_t late_packets;
  uint64_t reassembly_failures;
  uint64_t socket_errors;
} mezon_stats_t;

#ifdef __cplusplus
}
#endif

#endif
