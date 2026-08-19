#include "platform_android.h"

#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <stdlib.h>
#include <string.h>

static void copy_param(uint8_t **dst, size_t *dst_len, const uint8_t *src,
                       size_t len) {
  uint8_t *copy = (uint8_t *)realloc(*dst, len);
  if (!copy) {
    return;
  }
  memcpy(copy, src, len);
  *dst = copy;
  *dst_len = len;
}

static mezon_status_t configure_decoder(mezia_platform_t *p) {
  AMediaFormat *format;
  media_status_t st;
  if (p->decoder || !p->sps || !p->pps) {
    return p->decoder ? MEZON_OK : MEZON_ERR_NOT_READY;
  }
  p->decoder = AMediaCodec_createDecoderByType("video/avc");
  if (!p->decoder) {
    return MEZON_ERR_CODEC;
  }
  format = AMediaFormat_new();
  AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH,
                        (int32_t)p->config.video_width);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT,
                        (int32_t)p->config.video_height);
  AMediaFormat_setBuffer(format, "csd-0", p->sps, p->sps_len);
  AMediaFormat_setBuffer(format, "csd-1", p->pps, p->pps_len);
  st = AMediaCodec_configure((AMediaCodec *)p->decoder, format,
                             (ANativeWindow *)p->native_window, NULL, 0);
  AMediaFormat_delete(format);
  if (st != AMEDIA_OK) {
    AMediaCodec_delete((AMediaCodec *)p->decoder);
    p->decoder = NULL;
    return MEZON_ERR_CODEC;
  }
  if (AMediaCodec_start((AMediaCodec *)p->decoder) != AMEDIA_OK) {
    AMediaCodec_delete((AMediaCodec *)p->decoder);
    p->decoder = NULL;
    return MEZON_ERR_CODEC;
  }
  return MEZON_OK;
}

static void drain_decoder(mezia_platform_t *p) {
  AMediaCodecBufferInfo info;
  ssize_t index;
  AMediaCodec *codec = (AMediaCodec *)p->decoder;
  if (!codec) {
    return;
  }
  while ((index = AMediaCodec_dequeueOutputBuffer(codec, &info, 0)) >= 0) {
    AMediaCodec_releaseOutputBuffer(codec, (size_t)index, p->native_window != NULL);
  }
}

static void feed_nal(mezia_platform_t *p, const uint8_t *nal, size_t nal_len,
                     uint32_t rtp_timestamp) {
  AMediaCodec *codec;
  ssize_t index;
  size_t cap = 0;
  uint8_t *buf;
  uint8_t type;
  if (!nal || !nal_len) {
    return;
  }
  type = (uint8_t)(nal[0] & 0x1fU);
  if (type == 7U) {
    copy_param(&p->sps, &p->sps_len, nal, nal_len);
    return;
  }
  if (type == 8U) {
    copy_param(&p->pps, &p->pps_len, nal, nal_len);
    return;
  }
  if (configure_decoder(p) != MEZON_OK) {
    return;
  }
  codec = (AMediaCodec *)p->decoder;
  index = AMediaCodec_dequeueInputBuffer(codec, 2000);
  if (index < 0) {
    drain_decoder(p);
    return;
  }
  buf = AMediaCodec_getInputBuffer(codec, (size_t)index, &cap);
  if (!buf || cap < nal_len + 4U) {
    AMediaCodec_queueInputBuffer(codec, (size_t)index, 0, 0, 0, 0);
    return;
  }
  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 1;
  memcpy(buf + 4, nal, nal_len);
  AMediaCodec_queueInputBuffer(codec, (size_t)index, 0, nal_len + 4U,
                               (uint64_t)rtp_timestamp * 1000ULL / 90ULL, 0);
  drain_decoder(p);
}

mezon_status_t mezia_android_decoder_start(mezia_platform_t *platform) {
  (void)platform;
  return MEZON_OK;
}

void mezia_android_decoder_stop(mezia_platform_t *platform) {
  if (!platform || !platform->decoder) {
    return;
  }
  AMediaCodec_stop((AMediaCodec *)platform->decoder);
  AMediaCodec_delete((AMediaCodec *)platform->decoder);
  platform->decoder = NULL;
}

void mezia_android_decoder_on_nal(const uint8_t *nal, size_t nal_len,
                                  uint32_t rtp_timestamp,
                                  int end_of_access_unit, int discontinuity,
                                  void *user_data) {
  mezia_platform_t *p = (mezia_platform_t *)user_data;
  uint8_t *copy;
  (void)end_of_access_unit;
  (void)discontinuity;
  if (!p || !nal || !nal_len) {
    return;
  }
  copy = (uint8_t *)malloc(nal_len);
  if (!copy) {
    return;
  }
  memcpy(copy, nal, nal_len);
  feed_nal(p, copy, nal_len, rtp_timestamp);
  free(copy);
}
