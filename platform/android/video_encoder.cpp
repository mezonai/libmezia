#include "platform_android.h"
#include "yuv_convert.h"
#include "../common/nal_split.h"
#include "mezia/media.h"
#include "mezia_internal.h"

#include <media/NdkImage.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <stdlib.h>
#include <string.h>

#ifndef COLOR_FormatYUV420Flexible
#define COLOR_FormatYUV420Flexible 0x7f420888
#endif
#ifndef COLOR_FormatYUV420SemiPlanar
#define COLOR_FormatYUV420SemiPlanar 21
#endif

typedef struct {
  mezia_platform_t *platform;
  uint32_t rtp_timestamp;
} nal_ctx_t;

static void send_nal(const uint8_t *nal, size_t nal_len, int eoa, void *user) {
  nal_ctx_t *ctx = (nal_ctx_t *)user;
  if (nal && nal_len) {
    mezia_send_h264(ctx->platform->media, nal, nal_len, ctx->rtp_timestamp, eoa);
  }
}

static void drain_encoder(mezia_platform_t *p) {
  AMediaCodec *codec = (AMediaCodec *)p->encoder;
  AMediaCodecBufferInfo info;
  ssize_t index;
  if (!codec) {
    return;
  }
  while ((index = AMediaCodec_dequeueOutputBuffer(codec, &info, 0)) >= 0) {
    size_t out_size = 0;
    uint8_t *data = AMediaCodec_getOutputBuffer(codec, (size_t)index, &out_size);
    if (data && info.size > 0) {
      nal_ctx_t ctx;
      ctx.platform = p;
      ctx.rtp_timestamp = mezia_rtp_video_timestamp(mezon_clock_now_ns());
      mezia_split_annexb(data + info.offset, (size_t)info.size, send_nal, &ctx);
    }
    AMediaCodec_releaseOutputBuffer(codec, (size_t)index, false);
  }
}

mezon_status_t mezia_android_encoder_start(mezia_platform_t *platform) {
  AMediaFormat *format;
  media_status_t st;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->encoder = AMediaCodec_createEncoderByType("video/avc");
  if (!platform->encoder) {
    return MEZON_ERR_CODEC;
  }
  format = AMediaFormat_new();
  AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "video/avc");
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH,
                        (int32_t)platform->config.video_width);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT,
                        (int32_t)platform->config.video_height);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE,
                        (int32_t)platform->video_bitrate_bps);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE,
                        (int32_t)platform->config.video_fps);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 2);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                        COLOR_FormatYUV420SemiPlanar);
  st = AMediaCodec_configure((AMediaCodec *)platform->encoder, format, NULL,
                             NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
  AMediaFormat_delete(format);
  if (st != AMEDIA_OK) {
    AMediaCodec_delete((AMediaCodec *)platform->encoder);
    platform->encoder = NULL;
    return MEZON_ERR_CODEC;
  }
  if (AMediaCodec_start((AMediaCodec *)platform->encoder) != AMEDIA_OK) {
    AMediaCodec_delete((AMediaCodec *)platform->encoder);
    platform->encoder = NULL;
    return MEZON_ERR_CODEC;
  }
  return MEZON_OK;
}

void mezia_android_encoder_stop(mezia_platform_t *platform) {
  if (!platform || !platform->encoder) {
    return;
  }
  AMediaCodec_stop((AMediaCodec *)platform->encoder);
  AMediaCodec_delete((AMediaCodec *)platform->encoder);
  platform->encoder = NULL;
}

mezon_status_t mezia_android_encoder_push_image(mezia_platform_t *platform,
                                                void *image_ptr) {
  AImage *image = (AImage *)image_ptr;
  AMediaCodec *codec;
  ssize_t index;
  size_t buf_size = 0;
  uint8_t *buf;
  uint8_t *y = NULL;
  uint8_t *u = NULL;
  uint8_t *v = NULL;
  int y_len = 0, u_len = 0, v_len = 0;
  int32_t y_stride = 0, u_stride = 0, v_stride = 0;
  int32_t width = 0, height = 0;
  int32_t uv_pixel = 1;
  if (!platform || !platform->encoder || !image) {
    return MEZON_ERR_INVALID_ARG;
  }
  codec = (AMediaCodec *)platform->encoder;
  AImage_getWidth(image, &width);
  AImage_getHeight(image, &height);
  AImage_getPlaneData(image, 0, &y, &y_len);
  AImage_getPlaneData(image, 1, &u, &u_len);
  AImage_getPlaneData(image, 2, &v, &v_len);
  AImage_getPlaneRowStride(image, 0, &y_stride);
  AImage_getPlaneRowStride(image, 1, &u_stride);
  AImage_getPlaneRowStride(image, 2, &v_stride);
  AImage_getPlanePixelStride(image, 1, &uv_pixel);
  index = AMediaCodec_dequeueInputBuffer(codec, 0);
  if (index < 0) {
    drain_encoder(platform);
    return MEZON_ERR_WOULD_BLOCK;
  }
  buf = AMediaCodec_getInputBuffer(codec, (size_t)index, &buf_size);
  if (!buf || buf_size < (size_t)width * (size_t)height * 3U / 2U) {
    AMediaCodec_queueInputBuffer(codec, (size_t)index, 0, 0, 0, 0);
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  mezia_yuv420_888_to_nv12(buf, (size_t)width, y, (size_t)y_stride, u,
                           (size_t)u_stride, v, (size_t)v_stride, uv_pixel,
                           width, height);
  AMediaCodec_queueInputBuffer(codec, (size_t)index, 0,
                               (size_t)width * (size_t)height * 3U / 2U,
                               (uint64_t)mezon_clock_now_ns() / 1000ULL, 0);
  drain_encoder(platform);
  return MEZON_OK;
}

mezon_status_t mezia_android_encoder_set_bitrate(mezia_platform_t *platform,
                                                 uint32_t bitrate_bps) {
  AMediaCodec *codec;
  AMediaFormat *params;
  if (!platform || !platform->encoder) {
    return MEZON_ERR_NOT_READY;
  }
  codec = (AMediaCodec *)platform->encoder;
  params = AMediaFormat_new();
  AMediaFormat_setInt32(params, "video-bitrate", (int32_t)bitrate_bps);
  AMediaCodec_setParameters(codec, params);
  AMediaFormat_delete(params);
  return MEZON_OK;
}
