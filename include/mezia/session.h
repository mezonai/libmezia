#ifndef MEZIA_SESSION_H
#define MEZIA_SESSION_H

#include "mezia/audio.h"
#include "mezia/sdp.h"
#include "mezia/types.h"
#include "mezia/video.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezia_session mezia_session_t;

typedef struct {
  const char *local_ip;
  uint16_t local_port;
  const char *remote_ip;
  uint16_t remote_port;
  size_t mtu;
  int offer_audio;
  int offer_video;
  const char *ice_ufrag;
  const char *ice_pwd;
  const char *fingerprint;
  mezon_audio_callback_t on_audio;
  mezon_video_callback_t on_nal;
  void *user_data;
} mezia_session_config_t;

mezia_session_t *mezia_session_create(const mezia_session_config_t *config);
void mezia_session_close(mezia_session_t *session);

mezon_status_t mezia_session_create_offer(mezia_session_t *session, char *out,
                                          size_t out_capacity, size_t *out_len);
mezon_status_t mezia_session_set_local_description(mezia_session_t *session,
                                                   const char *sdp);
mezon_status_t mezia_session_set_remote_description(mezia_session_t *session,
                                                    const char *sdp);
mezon_status_t mezia_session_start(mezia_session_t *session);

mezon_status_t mezia_session_send_audio(mezia_session_t *session,
                                        const int16_t *pcm,
                                        size_t samples_per_channel);
mezon_status_t mezia_session_playout_audio(mezia_session_t *session,
                                           uint64_t now_ns);
mezon_status_t mezia_session_send_video(mezia_session_t *session,
                                        const uint8_t *nal, size_t nal_len,
                                        uint32_t rtp_timestamp,
                                        int end_of_access_unit);
void mezia_session_get_stats(const mezia_session_t *session,
                             mezon_stats_t *stats);
const mezia_sdp_t *mezia_session_local_sdp(const mezia_session_t *session);
const mezia_sdp_t *mezia_session_remote_sdp(const mezia_session_t *session);

#ifdef __cplusplus
}
#endif

#endif
