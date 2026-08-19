#include "platform_android.h"

#include "mezia/media.h"
#include "mezia_internal.h"

#include <aaudio/AAudio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static aaudio_data_callback_result_t capture_cb(AAudioStream *stream,
                                                void *userData, void *audioData,
                                                int32_t numFrames) {
  mezia_platform_t *p = (mezia_platform_t *)userData;
  (void)stream;
  pthread_mutex_lock(&p->audio_lock);
  mezia_audio_ring_write(&p->capture_ring, (const int16_t *)audioData,
                         (size_t)numFrames);
  pthread_mutex_unlock(&p->audio_lock);
  drain_capture(p);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static aaudio_data_callback_result_t playback_cb(AAudioStream *stream,
                                                 void *userData, void *audioData,
                                                 int32_t numFrames) {
  mezia_platform_t *p = (mezia_platform_t *)userData;
  size_t got;
  (void)stream;
  pthread_mutex_lock(&p->audio_lock);
  got = mezia_audio_ring_read(&p->playback_ring, (int16_t *)audioData,
                              (size_t)numFrames);
  pthread_mutex_unlock(&p->audio_lock);
  if (got < (size_t)numFrames) {
    memset((int16_t *)audioData + got, 0,
           ((size_t)numFrames - got) * sizeof(int16_t));
  }
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void error_cb(AAudioStream *stream, void *userData, aaudio_result_t error) {
  mezia_platform_t *p = (mezia_platform_t *)userData;
  (void)stream;
  (void)error;
  mezia_platform_report_error(p, MEZON_ERR_INTERNAL, "aaudio");
}

static AAudioStream *open_stream(mezia_platform_t *p, aaudio_direction_t dir,
                                 AAudioStream_dataCallback cb) {
  AAudioStreamBuilder *builder = NULL;
  AAudioStream *stream = NULL;
  if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) {
    return NULL;
  }
  AAudioStreamBuilder_setDirection(builder, dir);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setChannelCount(builder, 1);
  AAudioStreamBuilder_setSampleRate(builder, 48000);
  AAudioStreamBuilder_setFramesPerDataCallback(builder, 960);
  AAudioStreamBuilder_setDataCallback(builder, cb, p);
  AAudioStreamBuilder_setErrorCallback(builder, error_cb, p);
  if (dir == AAUDIO_DIRECTION_INPUT) {
    AAudioStreamBuilder_setInputPreset(builder,
                                       AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
  }
  if (AAudioStreamBuilder_openStream(builder, &stream) != AAUDIO_OK) {
    AAudioStreamBuilder_delete(builder);
    return NULL;
  }
  AAudioStreamBuilder_delete(builder);
  return stream;
}

static void *playout_loop(void *arg) {
  mezia_platform_t *p = (mezia_platform_t *)arg;
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 20 * 1000 * 1000;
  while (p->playout_running) {
    if (p->running) {
      (void)mezia_playout_audio(p->media, mezon_clock_now_ns());
    }
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
  }
  return NULL;
}

mezon_status_t mezia_android_audio_start(mezia_platform_t *platform) {
  AAudioStream *in;
  AAudioStream *out;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  in = open_stream(platform, AAUDIO_DIRECTION_INPUT, capture_cb);
  out = open_stream(platform, AAUDIO_DIRECTION_OUTPUT, playback_cb);
  if (!in || !out) {
    if (in) {
      AAudioStream_close(in);
    }
    if (out) {
      AAudioStream_close(out);
    }
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "aaudio.open");
    return MEZON_ERR_INTERNAL;
  }
  platform->capture_stream = in;
  platform->playback_stream = out;
  if (AAudioStream_requestStart(in) != AAUDIO_OK ||
      AAudioStream_requestStart(out) != AAUDIO_OK) {
    mezia_android_audio_stop(platform);
    return MEZON_ERR_INTERNAL;
  }
  platform->playout_running = 1;
  if (pthread_create(&platform->playout_thread, NULL, playout_loop, platform) !=
      0) {
    platform->playout_running = 0;
    mezia_android_audio_stop(platform);
    return MEZON_ERR_INTERNAL;
  }
  return MEZON_OK;
}

void mezia_android_audio_stop(mezia_platform_t *platform) {
  if (!platform) {
    return;
  }
  if (platform->playout_running) {
    platform->playout_running = 0;
    pthread_join(platform->playout_thread, NULL);
  }
  if (platform->capture_stream) {
    AAudioStream_requestStop((AAudioStream *)platform->capture_stream);
    AAudioStream_close((AAudioStream *)platform->capture_stream);
    platform->capture_stream = NULL;
  }
  if (platform->playback_stream) {
    AAudioStream_requestStop((AAudioStream *)platform->playback_stream);
    AAudioStream_close((AAudioStream *)platform->playback_stream);
    platform->playback_stream = NULL;
  }
}
