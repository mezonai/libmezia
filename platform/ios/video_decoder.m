#import <CoreMedia/CoreMedia.h>
#import <VideoToolbox/VideoToolbox.h>

#include "platform_ios.h"

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

static void decompression_callback(void *decompressionOutputRefCon,
                                   void *sourceFrameRefCon, OSStatus status,
                                   VTDecodeInfoFlags infoFlags,
                                   CVImageBufferRef imageBuffer,
                                   CMTime presentationTimeStamp,
                                   CMTime presentationDuration) {
  mezia_platform_t *p = (mezia_platform_t *)decompressionOutputRefCon;
  (void)sourceFrameRefCon;
  (void)infoFlags;
  (void)presentationDuration;
  if (status != noErr || !imageBuffer || !p || !p->config.on_decoded_frame) {
    return;
  }
  p->config.on_decoded_frame(
      imageBuffer, (uint32_t)CMTimeGetSeconds(presentationTimeStamp) * 90000U,
      p->config.user_data);
}

static mezon_status_t ensure_session(mezia_platform_t *p) {
  OSStatus status;
  const uint8_t *params[2];
  size_t sizes[2];
  VTDecompressionOutputCallbackRecord cb;
  if (p->decompression || !p->sps || !p->pps) {
    return p->decompression ? MEZON_OK : MEZON_ERR_NOT_READY;
  }
  params[0] = p->sps;
  sizes[0] = p->sps_len;
  params[1] = p->pps;
  sizes[1] = p->pps_len;
  status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault, 2, params, sizes, 4, &p->format_desc);
  if (status != noErr) {
    return MEZON_ERR_CODEC;
  }
  cb.decompressionOutputCallback = decompression_callback;
  cb.decompressionOutputRefCon = p;
  status = VTDecompressionSessionCreate(kCFAllocatorDefault, p->format_desc,
                                        NULL, NULL, &cb, &p->decompression);
  return status == noErr ? MEZON_OK : MEZON_ERR_CODEC;
}

static void decode_nal(mezia_platform_t *p, const uint8_t *nal, size_t nal_len,
                       uint32_t rtp_timestamp) {
  uint8_t type;
  CMBlockBufferRef block = NULL;
  CMSampleBufferRef sample = NULL;
  CMSampleTimingInfo timing;
  size_t total;
  OSStatus status;
  if (!nal || nal_len == 0) {
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
  if (ensure_session(p) != MEZON_OK) {
    return;
  }
  total = 4U + nal_len;
  if (p->decode_annexb_cap < total) {
    uint8_t *n = (uint8_t *)realloc(p->decode_annexb, total);
    if (!n) {
      return;
    }
    p->decode_annexb = n;
    p->decode_annexb_cap = total;
  }
  p->decode_annexb[0] = 0;
  p->decode_annexb[1] = 0;
  p->decode_annexb[2] = 0;
  p->decode_annexb[3] = 1;
  memcpy(p->decode_annexb + 4U, nal, nal_len);
  if (CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, p->decode_annexb,
                                         total, kCFAllocatorNull, NULL, 0,
                                         total, 0, &block) != noErr) {
    return;
  }
  memset(&timing, 0, sizeof(timing));
  timing.duration = CMTimeMake(1, (int32_t)p->config.video_fps);
  timing.presentationTimeStamp =
      CMTimeMake((int64_t)rtp_timestamp, 90000);
  status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, p->format_desc,
                                     1, 1, &timing, 1, &total, &sample);
  CFRelease(block);
  if (status != noErr || !sample) {
    return;
  }
  VTDecompressionSessionDecodeFrame(p->decompression, sample, 0, NULL, NULL);
  CFRelease(sample);
}

mezon_status_t mezia_ios_decoder_start(mezia_platform_t *platform) {
  (void)platform;
  return MEZON_OK;
}

void mezia_ios_decoder_stop(mezia_platform_t *platform) {
  if (!platform) {
    return;
  }
  if (platform->decompression) {
    VTDecompressionSessionInvalidate(platform->decompression);
    CFRelease(platform->decompression);
    platform->decompression = NULL;
  }
  if (platform->format_desc) {
    CFRelease(platform->format_desc);
    platform->format_desc = NULL;
  }
}

void mezia_ios_decoder_on_nal(const uint8_t *nal, size_t nal_len,
                              uint32_t rtp_timestamp, int end_of_access_unit,
                              int discontinuity, void *user_data) {
  mezia_platform_t *p = (mezia_platform_t *)user_data;
  uint8_t *copy;
  (void)end_of_access_unit;
  if (!p || !nal || !nal_len) {
    return;
  }
  if (discontinuity && p->decompression) {
    VTDecompressionSessionWaitForAsynchronousFrames(p->decompression);
  }
  copy = (uint8_t *)malloc(nal_len);
  if (!copy) {
    return;
  }
  memcpy(copy, nal, nal_len);
  dispatch_async(p->decode_queue, ^{
    decode_nal(p, copy, nal_len, rtp_timestamp);
    free(copy);
  });
}
