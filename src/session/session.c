#include "mezia/session.h"
#include "mezia/media.h"

#include <stdlib.h>
#include <string.h>

struct mezia_session {
  mezia_session_config_t config;
  char local_ip[64];
  char remote_ip[64];
  char ice_ufrag[MEZIA_SDP_MAX_TOKEN];
  char ice_pwd[MEZIA_SDP_MAX_TOKEN];
  char fingerprint[MEZIA_SDP_MAX_TOKEN];
  mezia_sdp_t local;
  mezia_sdp_t remote;
  int have_local;
  int have_remote;
  int started;
  mezia_ctx_t *media;
  mezon_audio_config_t audio_cfg;
  mezon_video_config_t video_cfg;
};

static void copy_cstr(char *dst, size_t dst_size, const char *src,
                      const char *fallback) {
  const char *value = src && src[0] ? src : fallback;
  size_t n = strlen(value);
  if (n >= dst_size) {
    n = dst_size - 1;
  }
  memcpy(dst, value, n);
  dst[n] = '\0';
}

mezia_session_t *mezia_session_create(const mezia_session_config_t *config) {
  mezia_session_t *session;
  if (!config || !config->local_ip || !config->remote_ip) {
    return NULL;
  }
  if (!config->offer_audio && !config->offer_video) {
    return NULL;
  }
  session = (mezia_session_t *)calloc(1, sizeof(*session));
  if (!session) {
    return NULL;
  }
  session->config = *config;
  copy_cstr(session->local_ip, sizeof(session->local_ip), config->local_ip,
            "127.0.0.1");
  copy_cstr(session->remote_ip, sizeof(session->remote_ip), config->remote_ip,
            "127.0.0.1");
  copy_cstr(session->ice_ufrag, sizeof(session->ice_ufrag), config->ice_ufrag,
            "mezia");
  copy_cstr(session->ice_pwd, sizeof(session->ice_pwd), config->ice_pwd,
            "meziapasswordmeziapassword");
  copy_cstr(session->fingerprint, sizeof(session->fingerprint),
            config->fingerprint,
            "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:"
            "00:00:00:00:00:00:00:00:00:00:00");
  session->config.local_ip = session->local_ip;
  session->config.remote_ip = session->remote_ip;
  session->config.ice_ufrag = session->ice_ufrag;
  session->config.ice_pwd = session->ice_pwd;
  session->config.fingerprint = session->fingerprint;
  return session;
}

void mezia_session_close(mezia_session_t *session) {
  if (!session) {
    return;
  }
  mezia_destroy(session->media);
  free(session);
}

mezon_status_t mezia_session_create_offer(mezia_session_t *session, char *out,
                                          size_t out_capacity, size_t *out_len) {
  mezia_sdp_offer_params_t params;
  if (!session) {
    return MEZON_ERR_INVALID_ARG;
  }
  memset(&params, 0, sizeof(params));
  params.ice_ufrag = session->ice_ufrag;
  params.ice_pwd = session->ice_pwd;
  params.fingerprint_algo = "sha-256";
  params.fingerprint = session->fingerprint;
  params.offer_audio = session->config.offer_audio;
  params.offer_video = session->config.offer_video;
  params.audio_payload_type = 111U;
  params.video_payload_type = 102U;
  params.audio_mid = "0";
  params.video_mid = "1";
  return mezia_sdp_write_offer(&params, out, out_capacity, out_len);
}

mezon_status_t mezia_session_set_local_description(mezia_session_t *session,
                                                   const char *sdp) {
  mezon_status_t status;
  if (!session || !sdp || session->started) {
    return session && session->started ? MEZON_ERR_STATE
                                       : MEZON_ERR_INVALID_ARG;
  }
  status = mezia_sdp_parse(sdp, &session->local);
  if (status != MEZON_OK) {
    return status;
  }
  session->have_local = 1;
  return MEZON_OK;
}

mezon_status_t mezia_session_set_remote_description(mezia_session_t *session,
                                                    const char *sdp) {
  mezon_status_t status;
  if (!session || !sdp || session->started) {
    return session && session->started ? MEZON_ERR_STATE
                                       : MEZON_ERR_INVALID_ARG;
  }
  status = mezia_sdp_parse(sdp, &session->remote);
  if (status != MEZON_OK) {
    return status;
  }
  session->have_remote = 1;
  return MEZON_OK;
}

static uint8_t negotiated_pt(const mezia_sdp_media_t *local,
                             const mezia_sdp_media_t *remote,
                             uint8_t fallback) {
  if (remote && remote->present && remote->payload_type) {
    return remote->payload_type;
  }
  if (local && local->present && local->payload_type) {
    return local->payload_type;
  }
  return fallback;
}

mezon_status_t mezia_session_start(mezia_session_t *session) {
  mezia_config_t media_cfg;
  mezon_peer_config_t peer;
  if (!session) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (session->started) {
    return MEZON_OK;
  }
  if (!session->have_local || !session->have_remote) {
    return MEZON_ERR_NOT_READY;
  }
  memset(&media_cfg, 0, sizeof(media_cfg));
  memset(&peer, 0, sizeof(peer));
  peer.local_ip = session->local_ip;
  peer.local_port = session->config.local_port;
  peer.remote_ip = session->remote_ip;
  peer.remote_port = session->config.remote_port;
  peer.mtu = session->config.mtu ? session->config.mtu : MEZON_DEFAULT_MTU;

  if (session->config.offer_audio && session->local.audio.present &&
      session->remote.audio.present) {
    memset(&session->audio_cfg, 0, sizeof(session->audio_cfg));
    session->audio_cfg.payload_type =
        negotiated_pt(&session->local.audio, &session->remote.audio, 111U);
    session->audio_cfg.ssrc = 0x11111111U;
    session->audio_cfg.mtu = peer.mtu;
    session->audio_cfg.jitter_target_ms = 60;
    session->audio_cfg.jitter_max_ms = 120;
    session->audio_cfg.on_audio = session->config.on_audio;
    session->audio_cfg.user_data = session->config.user_data;
    media_cfg.audio = &session->audio_cfg;
  }
  if (session->config.offer_video && session->local.video.present &&
      session->remote.video.present) {
    memset(&session->video_cfg, 0, sizeof(session->video_cfg));
    session->video_cfg.payload_type =
        negotiated_pt(&session->local.video, &session->remote.video, 102U);
    session->video_cfg.ssrc = 0x22222222U;
    session->video_cfg.mtu = peer.mtu;
    session->video_cfg.max_nal_size = 256 * 1024;
    session->video_cfg.on_nal = session->config.on_nal;
    session->video_cfg.user_data = session->config.user_data;
    media_cfg.video = &session->video_cfg;
  }
  if (!media_cfg.audio && !media_cfg.video) {
    return MEZON_ERR_UNSUPPORTED;
  }
  media_cfg.peer = peer;
  session->media = mezia_create(&media_cfg);
  if (!session->media) {
    return MEZON_ERR_INTERNAL;
  }
  if (mezia_start(session->media) != MEZON_OK) {
    mezia_destroy(session->media);
    session->media = NULL;
    return MEZON_ERR_NETWORK;
  }
  session->started = 1;
  return MEZON_OK;
}

mezon_status_t mezia_session_send_audio(mezia_session_t *session,
                                        const int16_t *pcm,
                                        size_t samples_per_channel) {
  if (!session || !session->media) {
    return MEZON_ERR_NOT_READY;
  }
  return mezia_send_audio(session->media, pcm, samples_per_channel);
}

mezon_status_t mezia_session_playout_audio(mezia_session_t *session,
                                           uint64_t now_ns) {
  if (!session || !session->media) {
    return MEZON_ERR_NOT_READY;
  }
  return mezia_playout_audio(session->media, now_ns);
}

mezon_status_t mezia_session_send_video(mezia_session_t *session,
                                        const uint8_t *nal, size_t nal_len,
                                        uint32_t rtp_timestamp,
                                        int end_of_access_unit) {
  if (!session || !session->media) {
    return MEZON_ERR_NOT_READY;
  }
  return mezia_send_h264(session->media, nal, nal_len, rtp_timestamp,
                         end_of_access_unit);
}

void mezia_session_get_stats(const mezia_session_t *session,
                             mezon_stats_t *stats) {
  if (!stats) {
    return;
  }
  memset(stats, 0, sizeof(*stats));
  if (session && session->media) {
    mezia_get_stats(session->media, stats);
  }
}

const mezia_sdp_t *mezia_session_local_sdp(const mezia_session_t *session) {
  return session && session->have_local ? &session->local : NULL;
}

const mezia_sdp_t *mezia_session_remote_sdp(const mezia_session_t *session) {
  return session && session->have_remote ? &session->remote : NULL;
}
