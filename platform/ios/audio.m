#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "mezia/media.h"
#include "mezia_internal.h"
#include "platform_ios.h"

#include <string.h>

static void drain_capture(mezia_platform_t *p) {
  for (;;) {
    pthread_mutex_lock(&p->audio_lock);
    if (mezia_audio_ring_available(&p->capture_ring) < MEZON_OPUS_FRAME_SAMPLES) {
      pthread_mutex_unlock(&p->audio_lock);
      break;
    }
    mezia_audio_ring_read(&p->capture_ring, p->capture_frame,
                          MEZON_OPUS_FRAME_SAMPLES);
    pthread_mutex_unlock(&p->audio_lock);
    mezia_send_audio(p->media, p->capture_frame, MEZON_OPUS_FRAME_SAMPLES);
  }
}

static void playout_tick(mezia_platform_t *p) {
  if (!p->running) {
    return;
  }
  (void)mezia_playout_audio(p->media, mezon_clock_now_ns());
}

static AVAudioFormat *pcm16_mono_48k(void) {
  AudioStreamBasicDescription asbd;
  memset(&asbd, 0, sizeof(asbd));
  asbd.mSampleRate = 48000.0;
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  asbd.mBitsPerChannel = 16;
  asbd.mChannelsPerFrame = 1;
  asbd.mBytesPerFrame = 2;
  asbd.mFramesPerPacket = 1;
  asbd.mBytesPerPacket = 2;
  return [[AVAudioFormat alloc] initWithStreamDescription:&asbd];
}

mezon_status_t mezia_ios_audio_start(mezia_platform_t *platform) {
  NSError *error = nil;
  AVAudioSession *session;
  AVAudioFormat *hw;
  AVAudioFormat *target;
  AVAudioInputNode *input;
  AVAudioSourceNode *source;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  session = [AVAudioSession sharedInstance];
  if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                       mode:AVAudioSessionModeVoiceChat
                    options:AVAudioSessionCategoryOptionAllowBluetooth |
                            AVAudioSessionCategoryOptionDefaultToSpeaker
                      error:&error]) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "audio.session");
    return MEZON_ERR_INTERNAL;
  }
  [session setPreferredSampleRate:48000.0 error:nil];
  [session setPreferredIOBufferDuration:0.02 error:nil];
  if (![session setActive:YES error:&error]) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "audio.active");
    return MEZON_ERR_INTERNAL;
  }

  platform->engine = [[AVAudioEngine alloc] init];
  input = platform->engine.inputNode;
  hw = [input outputFormatForBus:0];
  target = pcm16_mono_48k();
  if (hw.sampleRate != 48000.0 || hw.channelCount != 1 ||
      hw.commonFormat != AVAudioPCMFormatInt16) {
    platform->converter =
        [[AVAudioConverter alloc] initFromFormat:hw toFormat:target];
  }

  [input installTapOnBus:0
              bufferSize:960
                  format:hw
                   block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
                     int16_t scratch[2048];
                     AVAudioPCMBuffer *converted = buffer;
                     AVAudioFrameCount frames;
                     (void)when;
                     if (platform->converter) {
                       AVAudioPCMBuffer *out = [[AVAudioPCMBuffer alloc]
                           initWithPCMFormat:target
                               frameCapacity:2048];
                       NSError *convErr = nil;
                       AVAudioConverterInputBlock inBlock =
                           ^AVAudioBuffer *_Nullable(
                               AVAudioPacketCount inNumberOfPackets,
                               AVAudioConverterInputStatus *outStatus) {
                             (void)inNumberOfPackets;
                             *outStatus = AVAudioConverterInputStatus_HaveData;
                             return buffer;
                           };
                       [platform->converter convertToBuffer:out
                                                      error:&convErr
                                         withInputFromBlock:inBlock];
                       converted = out;
                     }
                     frames = converted.frameLength;
                     if (!converted.int16ChannelData || frames == 0) {
                       return;
                     }
                     if (frames > 2048) {
                       frames = 2048;
                     }
                     memcpy(scratch, converted.int16ChannelData[0],
                            (size_t)frames * sizeof(int16_t));
                     pthread_mutex_lock(&platform->audio_lock);
                     mezia_audio_ring_write(&platform->capture_ring, scratch,
                                            frames);
                     pthread_mutex_unlock(&platform->audio_lock);
                     dispatch_async(platform->audio_send_queue, ^{
                       drain_capture(platform);
                     });
                   }];

  source = [[AVAudioSourceNode alloc]
      initWithFormat:target
         renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                               AVAudioFrameCount frameCount,
                               AudioBufferList *outputData) {
           int16_t tmp[2048];
           size_t got;
           (void)timestamp;
           if (frameCount > 2048) {
             frameCount = 2048;
           }
           pthread_mutex_lock(&platform->audio_lock);
           got = mezia_audio_ring_read(&platform->playback_ring, tmp,
                                       frameCount);
           pthread_mutex_unlock(&platform->audio_lock);
           while (got < frameCount) {
             tmp[got++] = 0;
           }
           *isSilence = NO;
           if (outputData->mNumberBuffers > 0 &&
               outputData->mBuffers[0].mData) {
             memcpy(outputData->mBuffers[0].mData, tmp,
                    (size_t)frameCount * sizeof(int16_t));
           }
           return noErr;
         }];
  [platform->engine attachNode:source];
  [platform->engine connect:source
                         to:platform->engine.mainMixerNode
                     format:target];

  if (![platform->engine startAndReturnError:&error]) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "audio.engine");
    return MEZON_ERR_INTERNAL;
  }

  platform->playout_timer = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_TIMER, 0, 0, platform->audio_playout_queue);
  dispatch_source_set_timer(platform->playout_timer,
                            dispatch_time(DISPATCH_TIME_NOW, 0),
                            20ull * NSEC_PER_MSEC, 1ull * NSEC_PER_MSEC);
  dispatch_source_set_event_handler(platform->playout_timer, ^{
    playout_tick(platform);
  });
  dispatch_resume(platform->playout_timer);
  return MEZON_OK;
}

void mezia_ios_audio_stop(mezia_platform_t *platform) {
  if (!platform) {
    return;
  }
  if (platform->playout_timer) {
    dispatch_source_cancel(platform->playout_timer);
    platform->playout_timer = nil;
  }
  if (platform->engine) {
    [platform->engine.inputNode removeTapOnBus:0];
    [platform->engine stop];
    platform->engine = nil;
  }
  platform->converter = nil;
  [[AVAudioSession sharedInstance] setActive:NO error:nil];
}
