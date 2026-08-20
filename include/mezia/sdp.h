#ifndef MEZIA_SDP_H
#define MEZIA_SDP_H

#include "mezia/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEZIA_SDP_MAX_TOKEN 256
#define MEZIA_SDP_MAX_MID 16
#define MEZIA_SDP_MAX_MIDS 4
#define MEZIA_SDP_MAX_CODEC 16
#define MEZIA_SDP_MAX_EXTMAP 8

typedef enum {
  MEZIA_SDP_DIR_SENDRECV = 0,
  MEZIA_SDP_DIR_SENDONLY = 1,
  MEZIA_SDP_DIR_RECVONLY = 2,
  MEZIA_SDP_DIR_INACTIVE = 3
} mezia_sdp_dir_t;

typedef struct {
  uint8_t id;
  char uri[96];
} mezia_sdp_extmap_t;

typedef struct {
  int present;
  char mid[MEZIA_SDP_MAX_MID];
  uint8_t payload_type;
  char codec[MEZIA_SDP_MAX_CODEC];
  uint32_t clock_rate;
  uint8_t channels;
  mezia_sdp_dir_t direction;
  int rtcp_mux;
  int nack;
  int pli;
  int transport_cc;
  mezia_sdp_extmap_t extmap[MEZIA_SDP_MAX_EXTMAP];
  size_t extmap_count;
} mezia_sdp_media_t;

typedef struct {
  char ice_ufrag[MEZIA_SDP_MAX_TOKEN];
  char ice_pwd[MEZIA_SDP_MAX_TOKEN];
  char fingerprint_algo[16];
  char fingerprint[MEZIA_SDP_MAX_TOKEN];
  char setup[16];
  int trickle;
  int bundle;
  char bundle_mids[MEZIA_SDP_MAX_MIDS][MEZIA_SDP_MAX_MID];
  size_t bundle_mid_count;
  mezia_sdp_media_t audio;
  mezia_sdp_media_t video;
} mezia_sdp_t;

typedef struct {
  const char *ice_ufrag;
  const char *ice_pwd;
  const char *fingerprint_algo;
  const char *fingerprint;
  int offer_audio;
  int offer_video;
  uint8_t audio_payload_type;
  uint8_t video_payload_type;
  const char *audio_mid;
  const char *video_mid;
} mezia_sdp_offer_params_t;

void mezia_sdp_init(mezia_sdp_t *sdp);
mezon_status_t mezia_sdp_parse(const char *text, mezia_sdp_t *sdp);
mezon_status_t mezia_sdp_write_offer(const mezia_sdp_offer_params_t *params,
                                     char *out, size_t out_capacity,
                                     size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
