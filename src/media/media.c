#include "mezia/media.h"

#include <stdlib.h>
#include <string.h>

struct mezia_ctx {
  mezon_peer_t *peer;
  mezon_audio_ctx_t *audio;
  mezon_video_ctx_t *video;
  uint8_t audio_payload_type;
  uint8_t video_payload_type;
  size_t mtu;
  int running;
};

static void media_packet(const mezon_packet_t *packet, void *user_data) {
  mezia_ctx_t *ctx = (mezia_ctx_t *)user_data;
  if (ctx->audio && packet->payload_type == ctx->audio_payload_type) {
    mezon_audio_receive(ctx->audio, packet);
  } else if (ctx->video && packet->payload_type == ctx->video_payload_type) {
    mezon_video_receive(ctx->video, packet);
  }
}

mezia_ctx_t *mezia_create(const mezia_config_t *config) {
  mezia_ctx_t *ctx;
  mezon_peer_config_t peer_config;
  if (!config) {
    return NULL;
  }
  ctx = (mezia_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }
  ctx->mtu = config->peer.mtu ? config->peer.mtu : MEZON_DEFAULT_MTU;
  if ((config->audio && config->audio->mtu &&
       config->audio->mtu != ctx->mtu) ||
      (config->video && config->video->mtu &&
       config->video->mtu != ctx->mtu)) {
    free(ctx);
    return NULL;
  }
  if (config->audio) {
    ctx->audio = mezon_audio_create(config->audio);
    if (!ctx->audio) {
      mezia_destroy(ctx);
      return NULL;
    }
    ctx->audio_payload_type = config->audio->payload_type
                                  ? config->audio->payload_type
                                  : MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  }
  if (config->video) {
    ctx->video = mezon_video_create(config->video);
    if (!ctx->video) {
      mezia_destroy(ctx);
      return NULL;
    }
    ctx->video_payload_type = config->video->payload_type
                                  ? config->video->payload_type
                                  : MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE;
  }
  peer_config = config->peer;
  peer_config.on_packet = media_packet;
  peer_config.user_data = ctx;
  ctx->peer = mezon_peer_create(&peer_config);
  if (!ctx->peer) {
    mezia_destroy(ctx);
    return NULL;
  }
  return ctx;
}

void mezia_destroy(mezia_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  mezia_stop(ctx);
  mezon_peer_destroy(ctx->peer);
  mezon_audio_destroy(ctx->audio);
  mezon_video_destroy(ctx->video);
  free(ctx);
}

mezon_status_t mezia_start(mezia_ctx_t *ctx) {
  mezon_status_t status;
  if (!ctx) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (ctx->running) {
    return MEZON_OK;
  }
  status = mezon_peer_start(ctx->peer);
  if (status == MEZON_OK) {
    ctx->running = 1;
  }
  return status;
}

mezon_status_t mezia_stop(mezia_ctx_t *ctx) {
  if (!ctx) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (!ctx->running) {
    return MEZON_OK;
  }
  ctx->running = 0;
  return mezon_peer_stop(ctx->peer);
}

static mezon_status_t send_packets(mezia_ctx_t *ctx, mezon_packet_t *packets,
                                   size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    mezon_status_t status = mezon_peer_send(ctx->peer, &packets[i]);
    if (status != MEZON_OK) {
      return status;
    }
  }
  return MEZON_OK;
}

mezon_status_t mezia_send_audio(mezia_ctx_t *ctx, const int16_t *pcm,
                                size_t samples_per_channel) {
  mezon_packet_t *packets;
  uint8_t *storage;
  size_t count;
  size_t i;
  mezon_status_t status;
  if (!ctx || !ctx->audio || !ctx->running) {
    return MEZON_ERR_NOT_READY;
  }
  count = mezon_audio_max_packets(ctx->audio, samples_per_channel);
  if (!count || count > SIZE_MAX / sizeof(*packets) ||
      count > SIZE_MAX / ctx->mtu) {
    return MEZON_ERR_INVALID_ARG;
  }
  packets = (mezon_packet_t *)calloc(count, sizeof(*packets));
  storage = (uint8_t *)malloc(count * ctx->mtu);
  if (!packets || !storage) {
    free(packets);
    free(storage);
    return MEZON_ERR_NOMEM;
  }
  for (i = 0; i < count; ++i) {
    packets[i].data = storage + i * ctx->mtu;
    packets[i].capacity = ctx->mtu;
  }
  status = mezon_audio_packetize(ctx->audio, pcm, samples_per_channel, packets,
                                 count, &count);
  if (status == MEZON_OK) {
    status = send_packets(ctx, packets, count);
  }
  free(storage);
  free(packets);
  return status;
}

mezon_status_t mezia_send_h264(mezia_ctx_t *ctx, const uint8_t *nal,
                               size_t nal_len, uint32_t rtp_timestamp,
                               int end_of_access_unit) {
  mezon_packet_t *packets;
  uint8_t *storage;
  size_t count;
  size_t i;
  mezon_status_t status;
  if (!ctx || !ctx->video || !ctx->running) {
    return MEZON_ERR_NOT_READY;
  }
  count = mezon_video_max_packets_for_nal(ctx->video, nal_len);
  if (!count || count > SIZE_MAX / sizeof(*packets) ||
      count > SIZE_MAX / ctx->mtu) {
    return MEZON_ERR_INVALID_ARG;
  }
  packets = (mezon_packet_t *)calloc(count, sizeof(*packets));
  storage = (uint8_t *)malloc(count * ctx->mtu);
  if (!packets || !storage) {
    free(packets);
    free(storage);
    return MEZON_ERR_NOMEM;
  }
  for (i = 0; i < count; ++i) {
    packets[i].data = storage + i * ctx->mtu;
    packets[i].capacity = ctx->mtu;
  }
  status = mezon_video_packetize_nal(ctx->video, nal, nal_len, rtp_timestamp,
                                     end_of_access_unit, packets, count, &count);
  if (status == MEZON_OK) {
    status = send_packets(ctx, packets, count);
  }
  free(storage);
  free(packets);
  return status;
}

void mezia_get_stats(const mezia_ctx_t *ctx, mezon_stats_t *stats) {
  if (!ctx || !stats) {
    return;
  }
  mezon_peer_get_stats(ctx->peer, stats);
}
