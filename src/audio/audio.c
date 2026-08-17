#include "mezia/audio.h"

struct mezon_audio_ctx {
  int sample_rate;
  int channels;
};

mezon_audio_ctx_t *mezon_audio_create(int sample_rate, int channels) {
  (void)sample_rate;
  (void)channels;
  return NULL;
}

void mezon_audio_destroy(mezon_audio_ctx_t *ctx) { (void)ctx; }

mezon_status_t mezon_audio_encode(mezon_audio_ctx_t *ctx, const int16_t *pcm,
                                  size_t samples, mezon_packet_t *out) {
  (void)ctx;
  (void)pcm;
  (void)samples;
  (void)out;
  return MEZON_ERR_NOT_READY;
}

mezon_status_t mezon_audio_decode(mezon_audio_ctx_t *ctx,
                                  const mezon_packet_t *in, int16_t *pcm_out,
                                  size_t *samples_out) {
  (void)ctx;
  (void)in;
  (void)pcm_out;
  (void)samples_out;
  return MEZON_ERR_NOT_READY;
}
