#ifndef MEZIA_AUDIO_RING_H
#define MEZIA_AUDIO_RING_H

#include "mezia/types.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEZIA_AUDIO_RING_FRAMES 10U
#define MEZIA_AUDIO_RING_SAMPLES (MEZIA_AUDIO_RING_FRAMES * MEZON_OPUS_FRAME_SAMPLES)

typedef struct {
  int16_t samples[MEZIA_AUDIO_RING_SAMPLES];
  size_t read;
  size_t write;
  size_t count;
} mezia_audio_ring_t;

static inline void mezia_audio_ring_init(mezia_audio_ring_t *ring) {
  memset(ring, 0, sizeof(*ring));
}

static inline size_t mezia_audio_ring_space(const mezia_audio_ring_t *ring) {
  return MEZIA_AUDIO_RING_SAMPLES - ring->count;
}

static inline size_t mezia_audio_ring_available(const mezia_audio_ring_t *ring) {
  return ring->count;
}

static inline size_t mezia_audio_ring_write(mezia_audio_ring_t *ring,
                                            const int16_t *pcm, size_t samples) {
  size_t i;
  size_t written = 0;
  for (i = 0; i < samples && ring->count < MEZIA_AUDIO_RING_SAMPLES; ++i) {
    ring->samples[ring->write] = pcm[i];
    ring->write = (ring->write + 1U) % MEZIA_AUDIO_RING_SAMPLES;
    ring->count++;
    written++;
  }
  return written;
}

static inline size_t mezia_audio_ring_read(mezia_audio_ring_t *ring, int16_t *pcm,
                                           size_t samples) {
  size_t i;
  size_t read = 0;
  for (i = 0; i < samples && ring->count > 0; ++i) {
    pcm[i] = ring->samples[ring->read];
    ring->read = (ring->read + 1U) % MEZIA_AUDIO_RING_SAMPLES;
    ring->count--;
    read++;
  }
  return read;
}

#ifdef __cplusplus
}
#endif

#endif
