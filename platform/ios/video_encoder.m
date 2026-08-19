#import <CoreMedia/CoreMedia.h>
#import <VideoToolbox/VideoToolbox.h>

#include "../common/nal_split.h"
#include "mezia/media.h"
#include "mezia_internal.h"
#include "platform_ios.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
  mezia_platform_t *platform;
  uint32_t rtp_timestamp;
} mezia_nal_send_ctx_t;

static void send_nal(const uint8_t *nal, size_t nal_len, int end_of_access_unit,
                     void *user_data) {
  mezia_nal_send_ctx_t *ctx = (mezia_nal_send_ctx_t *)user_data;
  if (!nal || !nal_len) {
    return;
  }
  mezia_send_h264(ctx->platform->media, nal, nal_len, ctx->rtp_timestamp,
                  end_of_access_unit);
}

static void emit_avcc_nals(const uint8_t *data, size_t len, int length_size,
                           uint32_t rtp_timestamp, mezia_platform_t *p) {
  size_t offset = 0;
  while (offset + (size_t)length_size <= len) {
    size_t nal_len = 0;
    size_t i;
    int last;
    for (i = 0; i < (size_t)length_size; ++i) {
      nal_len = (nal_len << 8) | data[offset + i];
    }
    offset += (size_t)length_size;
    if (offset + nal_len > len) {
      break;
    }
    last = (offset + nal_len >= len);
    mezia_send_h264(p->media, data + offset, nal_len, rtp_timestamp, last);
    offset += nal_len;
  }
}

static void compression_callback(void *outputCallbackRefCon,
                                 void *sourceFrameRefCon, OSStatus status,
                                 VTEncodeInfoFlags infoFlags,
                                 CMSampleBufferRef sampleBuffer) {
  mezia_platform_t *p = (mezia_platform_t *)outputCallbackRefCon;
  CMBlockBufferRef block;
  size_t total = 0;
  char *bytes = NULL;
  CMFormatDescriptionRef format;
  size_t param_count = 0;
  int length_size = 4;
  uint32_t rtp_timestamp;
  (void)sourceFrameRefCon;
  (void)infoFlags;
  if (status != noErr || !sampleBuffer || !p || !p->running) {
    return;
  }
  rtp_timestamp = mezia_rtp_video_timestamp(mezon_clock_now_ns());
  format = CMSampleBufferGetFormatDescription(sampleBuffer);
  if (format &&
      CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          format, 0, NULL, NULL, &param_count, &length_size) == noErr &&
      !p->sent_parameter_sets) {
    size_t i;
    for (i = 0; i < param_count; ++i) {
      const uint8_t *ps = NULL;
      size_t ps_len = 0;
      if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
              format, i, &ps, &ps_len, NULL, NULL) == noErr &&
          ps && ps_len) {
        mezia_send_h264(p->media, ps, ps_len, rtp_timestamp, 0);
      }
    }
    p->sent_parameter_sets = 1;
  }
  block = CMSampleBufferGetDataBuffer(sampleBuffer);
  if (!block) {
    return;
  }
  if (CMBlockBufferGetDataPointer(block, 0, NULL, &total, &bytes) != noErr ||
      !bytes || !total) {
    return;
  }
  if (mezia_start_code_len((const uint8_t *)bytes, total, 0)) {
    mezia_nal_send_ctx_t ctx;
    ctx.platform = p;
    ctx.rtp_timestamp = rtp_timestamp;
    mezia_split_annexb((const uint8_t *)bytes, total, send_nal, &ctx);
  } else {
    emit_avcc_nals((const uint8_t *)bytes, total, length_size, rtp_timestamp,
                   p);
  }
}

mezon_status_t mezia_ios_encoder_start(mezia_platform_t *platform) {
  OSStatus status;
  CFMutableDictionaryRef encoder_spec;
  CFMutableDictionaryRef source;
  CFNumberRef bitrate;
  int32_t width;
  int32_t height;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  width = (int32_t)platform->config.video_width;
  height = (int32_t)platform->config.video_height;
  encoder_spec = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(
      encoder_spec, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
      kCFBooleanTrue);
  source = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                     &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
  {
    int32_t pix = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    CFNumberRef pixn =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix);
    CFDictionarySetValue(source, kCVPixelBufferPixelFormatTypeKey, pixn);
    CFRelease(pixn);
  }
  status = VTCompressionSessionCreate(
      kCFAllocatorDefault, width, height, kCMVideoCodecType_H264, encoder_spec,
      source, NULL, compression_callback, platform, &platform->compression);
  CFRelease(encoder_spec);
  CFRelease(source);
  if (status != noErr || !platform->compression) {
    mezia_platform_report_error(platform, MEZON_ERR_CODEC, "vt.create");
    return MEZON_ERR_CODEC;
  }
  VTSessionSetProperty(platform->compression,
                       kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
  VTSessionSetProperty(platform->compression,
                       kVTCompressionPropertyKey_ProfileLevel,
                       kVTProfileLevel_H264_Baseline_AutoLevel);
  VTSessionSetProperty(platform->compression,
                       kVTCompressionPropertyKey_AllowFrameReordering,
                       kCFBooleanFalse);
  {
    int32_t fps = (int32_t)platform->config.video_fps;
    CFNumberRef n =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &fps);
    VTSessionSetProperty(platform->compression,
                         kVTCompressionPropertyKey_ExpectedFrameRate, n);
    CFRelease(n);
  }
  {
    int32_t gop = (int32_t)(platform->config.video_fps * 2U);
    CFNumberRef n =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &gop);
    VTSessionSetProperty(platform->compression,
                         kVTCompressionPropertyKey_MaxKeyFrameInterval, n);
    CFRelease(n);
  }
  bitrate = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                           &platform->video_bitrate_bps);
  VTSessionSetProperty(platform->compression,
                       kVTCompressionPropertyKey_AverageBitRate, bitrate);
  CFRelease(bitrate);
  VTCompressionSessionPrepareToEncodeFrames(platform->compression);
  platform->sent_parameter_sets = 0;
  return MEZON_OK;
}

void mezia_ios_encoder_stop(mezia_platform_t *platform) {
  if (!platform || !platform->compression) {
    return;
  }
  VTCompressionSessionCompleteFrames(platform->compression, kCMTimeInvalid);
  VTCompressionSessionInvalidate(platform->compression);
  CFRelease(platform->compression);
  platform->compression = NULL;
}

mezon_status_t mezia_ios_encoder_encode(mezia_platform_t *platform,
                                        void *sample_buffer) {
  CMSampleBufferRef sample = (CMSampleBufferRef)sample_buffer;
  CVImageBufferRef image;
  CMTime pts;
  OSStatus status;
  if (!platform || !platform->compression || !sample) {
    return MEZON_ERR_INVALID_ARG;
  }
  image = CMSampleBufferGetImageBuffer(sample);
  if (!image) {
    return MEZON_ERR_INVALID_ARG;
  }
  pts = CMSampleBufferGetPresentationTimeStamp(sample);
  status = VTCompressionSessionEncodeFrame(platform->compression, image, pts,
                                           kCMTimeInvalid, NULL, NULL, NULL);
  return status == noErr ? MEZON_OK : MEZON_ERR_CODEC;
}

mezon_status_t mezia_ios_encoder_set_bitrate(mezia_platform_t *platform,
                                             uint32_t bitrate_bps) {
  CFNumberRef bitrate;
  if (!platform || !platform->compression) {
    return MEZON_ERR_NOT_READY;
  }
  bitrate =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bitrate_bps);
  VTSessionSetProperty(platform->compression,
                       kVTCompressionPropertyKey_AverageBitRate, bitrate);
  CFRelease(bitrate);
  return MEZON_OK;
}
