#include "audio_internal.h"

#include <opus/opus.h>
#include <stdlib.h>

struct mezon_opus_codec {
  OpusEncoder *encoder;
  OpusDecoder *decoder;
  uint32_t bitrate;
  uint8_t loss_percent;
};

static mezon_status_t map_opus_error(int error) {
  if (error == OPUS_BUFFER_TOO_SMALL) {
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  if (error == OPUS_BAD_ARG) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (error == OPUS_INVALID_PACKET) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  return MEZON_ERR_CODEC;
}

static int configure_encoder(OpusEncoder *encoder, uint32_t bitrate) {
  return opus_encoder_ctl(encoder, OPUS_SET_BITRATE((opus_int32)bitrate)) ==
             OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_VBR(1)) == OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT(1)) == OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5)) == OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_DTX(1)) == OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1)) == OPUS_OK &&
         opus_encoder_ctl(
             encoder,
             OPUS_SET_PACKET_LOSS_PERC(MEZON_OPUS_EXPECTED_LOSS_PERCENT)) ==
             OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)) ==
             OPUS_OK &&
         opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16)) == OPUS_OK;
}

mezon_opus_codec_t *mezon_opus_create(uint32_t initial_bitrate) {
  mezon_opus_codec_t *codec;
  int error = OPUS_OK;
  codec = (mezon_opus_codec_t *)calloc(1, sizeof(*codec));
  if (!codec) {
    return NULL;
  }
  codec->encoder = opus_encoder_create(MEZON_OPUS_SAMPLE_RATE,
                                       MEZON_OPUS_CHANNELS,
                                       OPUS_APPLICATION_VOIP, &error);
  if (!codec->encoder || error != OPUS_OK ||
      !configure_encoder(codec->encoder, initial_bitrate)) {
    mezon_opus_destroy(codec);
    return NULL;
  }
  codec->decoder = opus_decoder_create(MEZON_OPUS_SAMPLE_RATE,
                                       MEZON_OPUS_CHANNELS, &error);
  if (!codec->decoder || error != OPUS_OK) {
    mezon_opus_destroy(codec);
    return NULL;
  }
  codec->bitrate = initial_bitrate;
  codec->loss_percent = MEZON_OPUS_EXPECTED_LOSS_PERCENT;
  return codec;
}

void mezon_opus_destroy(mezon_opus_codec_t *codec) {
  if (!codec) {
    return;
  }
  if (codec->encoder) {
    opus_encoder_destroy(codec->encoder);
  }
  if (codec->decoder) {
    opus_decoder_destroy(codec->decoder);
  }
  free(codec);
}

mezon_status_t mezon_opus_set_network_state(mezon_opus_codec_t *codec,
                                             uint32_t bitrate,
                                             uint8_t loss_percent) {
  uint32_t old_bitrate;
  if (!codec || bitrate < 6000U || bitrate > 510000U || loss_percent > 100U) {
    return MEZON_ERR_INVALID_ARG;
  }
  old_bitrate = codec->bitrate;
  if (opus_encoder_ctl(codec->encoder,
                       OPUS_SET_BITRATE((opus_int32)bitrate)) != OPUS_OK) {
    return MEZON_ERR_CODEC;
  }
  if (opus_encoder_ctl(codec->encoder,
                       OPUS_SET_PACKET_LOSS_PERC((int)loss_percent)) !=
      OPUS_OK) {
    opus_encoder_ctl(codec->encoder,
                     OPUS_SET_BITRATE((opus_int32)old_bitrate));
    return MEZON_ERR_CODEC;
  }
  codec->bitrate = bitrate;
  codec->loss_percent = loss_percent;
  return MEZON_OK;
}

mezon_status_t mezon_opus_encode(mezon_opus_codec_t *codec,
                                 const int16_t *pcm, uint8_t *output,
                                 size_t output_capacity, size_t *output_len) {
  int encoded;
  if (!codec || !pcm || !output || !output_len || output_capacity == 0) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (output_capacity > INT32_MAX) {
    output_capacity = INT32_MAX;
  }
  encoded = opus_encode(codec->encoder, pcm, MEZON_OPUS_FRAME_SAMPLES, output,
                        (opus_int32)output_capacity);
  if (encoded < 0) {
    return map_opus_error(encoded);
  }
  *output_len = (size_t)encoded;
  return MEZON_OK;
}

mezon_status_t mezon_opus_decode(mezon_opus_codec_t *codec,
                                 const uint8_t *payload, size_t payload_len,
                                 int decode_fec, int16_t *pcm) {
  int decoded;
  if (!codec || !pcm || payload_len > INT32_MAX) {
    return MEZON_ERR_INVALID_ARG;
  }
  decoded = opus_decode(codec->decoder, payload, (opus_int32)payload_len, pcm,
                        MEZON_OPUS_FRAME_SAMPLES, decode_fec);
  if (decoded < 0) {
    return map_opus_error(decoded);
  }
  return decoded == (int)MEZON_OPUS_FRAME_SAMPLES ? MEZON_OK
                                                   : MEZON_ERR_CODEC;
}
