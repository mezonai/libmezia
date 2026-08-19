#include "platform_android.h"

#include "mezia/media.h"
#include "mezia_internal.h"

#include <stdlib.h>
#include <string.h>

void mezia_platform_report_error(mezia_platform_t *platform,
                                 mezon_status_t status, const char *where) {
  if (platform && platform->config.on_error) {
    platform->config.on_error(status, where, platform->config.user_data);
  }
}

static void on_decoded_audio(const int16_t *pcm, size_t samples,
                             uint32_t rtp_timestamp, int discontinuity,
                             void *user_data) {
  mezia_platform_t *p = (mezia_platform_t *)user_data;
  (void)rtp_timestamp;
  (void)discontinuity;
  pthread_mutex_lock(&p->audio_lock);
  mezia_audio_ring_write(&p->playback_ring, pcm, samples);
  pthread_mutex_unlock(&p->audio_lock);
}

static void fill_audio_config(mezia_platform_t *p) {
  memset(&p->audio_config, 0, sizeof(p->audio_config));
  p->audio_config.payload_type = p->config.audio_payload_type
                                     ? p->config.audio_payload_type
                                     : MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  p->audio_config.ssrc = p->config.audio_ssrc;
  p->audio_config.mtu = p->config.peer.mtu;
  p->audio_config.jitter_target_ms =
      p->config.jitter_target_ms ? p->config.jitter_target_ms : 60U;
  p->audio_config.jitter_max_ms =
      p->config.jitter_max_ms ? p->config.jitter_max_ms : 120U;
  p->audio_config.adaptation.enabled = p->config.adaptation_enabled;
  p->audio_config.adaptation.min_bitrate_bps =
      p->config.min_audio_bitrate_bps ? p->config.min_audio_bitrate_bps : 16000U;
  p->audio_config.adaptation.initial_bitrate_bps =
      p->config.initial_audio_bitrate_bps ? p->config.initial_audio_bitrate_bps
                                          : MEZON_OPUS_BITRATE;
  p->audio_config.adaptation.max_bitrate_bps =
      p->config.max_audio_bitrate_bps ? p->config.max_audio_bitrate_bps : 48000U;
  p->audio_config.on_audio = on_decoded_audio;
  p->audio_config.user_data = p;
}

static void fill_video_config(mezia_platform_t *p) {
  memset(&p->video_config, 0, sizeof(p->video_config));
  p->video_config.payload_type = p->config.video_payload_type
                                     ? p->config.video_payload_type
                                     : MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE;
  p->video_config.ssrc = p->config.video_ssrc;
  p->video_config.mtu = p->config.peer.mtu;
  p->video_config.max_nal_size =
      p->config.max_nal_size ? p->config.max_nal_size : (2U * 1024U * 1024U);
  p->video_config.on_nal = mezia_android_decoder_on_nal;
  p->video_config.user_data = p;
}

mezia_platform_t *mezia_platform_create(const mezia_platform_config_t *config) {
  mezia_platform_t *p;
  mezia_config_t media_cfg;
  if (!config) {
    return NULL;
  }
  p = (mezia_platform_t *)calloc(1, sizeof(*p));
  if (!p) {
    return NULL;
  }
  p->config = *config;
  if (!p->config.video_width) {
    p->config.video_width = 1280;
  }
  if (!p->config.video_height) {
    p->config.video_height = 720;
  }
  if (!p->config.video_fps) {
    p->config.video_fps = 30;
  }
  p->video_bitrate_bps =
      p->config.video_bitrate_bps ? p->config.video_bitrate_bps : 1500000U;
  if (!config->enable_audio && !config->enable_video) {
    p->config.enable_audio = 1;
    p->config.enable_video = 1;
  }
  p->native_window = config->render_target;
  pthread_mutex_init(&p->audio_lock, NULL);
  mezia_audio_ring_init(&p->capture_ring);
  mezia_audio_ring_init(&p->playback_ring);
  fill_audio_config(p);
  fill_video_config(p);
  memset(&media_cfg, 0, sizeof(media_cfg));
  media_cfg.peer = config->peer;
  media_cfg.audio = p->config.enable_audio ? &p->audio_config : NULL;
  media_cfg.video = p->config.enable_video ? &p->video_config : NULL;
  media_cfg.control_payload_type = config->control_payload_type;
  p->media = mezia_create(&media_cfg);
  if (!p->media) {
    mezia_platform_destroy(p);
    return NULL;
  }
  return p;
}

void mezia_platform_destroy(mezia_platform_t *platform) {
  if (!platform) {
    return;
  }
  mezia_platform_stop(platform);
  mezia_destroy(platform->media);
  pthread_mutex_destroy(&platform->audio_lock);
  free(platform->sps);
  free(platform->pps);
  free(platform->yuv_scratch);
  free(platform);
}

mezon_status_t mezia_platform_start(mezia_platform_t *platform) {
  mezon_status_t status;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (platform->running) {
    return MEZON_OK;
  }
  status = mezia_start(platform->media);
  if (status != MEZON_OK) {
    return status;
  }
  if (platform->config.enable_audio) {
    status = mezia_android_audio_start(platform);
    if (status != MEZON_OK) {
      mezia_stop(platform->media);
      return status;
    }
  }
  if (platform->config.enable_video) {
    status = mezia_android_decoder_start(platform);
    if (status != MEZON_OK) {
      mezia_android_audio_stop(platform);
      mezia_stop(platform->media);
      return status;
    }
    status = mezia_android_encoder_start(platform);
    if (status != MEZON_OK) {
      mezia_android_decoder_stop(platform);
      mezia_android_audio_stop(platform);
      mezia_stop(platform->media);
      return status;
    }
    status = mezia_android_camera_start(platform);
    if (status != MEZON_OK) {
      mezia_android_encoder_stop(platform);
      mezia_android_decoder_stop(platform);
      mezia_android_audio_stop(platform);
      mezia_stop(platform->media);
      return status;
    }
  }
  platform->running = 1;
  return MEZON_OK;
}

mezon_status_t mezia_platform_stop(mezia_platform_t *platform) {
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->running = 0;
  mezia_android_camera_stop(platform);
  mezia_android_encoder_stop(platform);
  mezia_android_decoder_stop(platform);
  mezia_android_audio_stop(platform);
  return platform->media ? mezia_stop(platform->media) : MEZON_OK;
}

mezon_status_t mezia_platform_set_video_bitrate(mezia_platform_t *platform,
                                                uint32_t bitrate_bps) {
  if (!platform || bitrate_bps < 100000U) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->video_bitrate_bps = bitrate_bps;
  platform->config.video_bitrate_bps = bitrate_bps;
  return mezia_android_encoder_set_bitrate(platform, bitrate_bps);
}

mezon_status_t mezia_platform_set_camera(mezia_platform_t *platform,
                                         mezia_camera_facing_t facing) {
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  return mezia_android_camera_set_facing(platform, facing);
}

mezon_status_t mezia_platform_set_render_target(mezia_platform_t *platform,
                                                void *native_window) {
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->native_window = native_window;
  platform->config.render_target = native_window;
  return MEZON_OK;
}

void mezia_platform_get_stats(const mezia_platform_t *platform,
                              mezon_stats_t *stats) {
  if (!platform || !stats) {
    return;
  }
  mezia_get_stats(platform->media, stats);
}

mezia_ctx_t *mezia_platform_media(mezia_platform_t *platform) {
  return platform ? platform->media : NULL;
}
