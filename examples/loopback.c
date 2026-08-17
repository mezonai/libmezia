#include "mezia/audio.h"
#include "mezia/video.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  mezon_audio_config_t audio_config;
  mezon_video_config_t video_config;
  mezon_audio_ctx_t *audio;
  mezon_video_ctx_t *video;
  memset(&audio_config, 0, sizeof(audio_config));
  memset(&video_config, 0, sizeof(video_config));
  audio_config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  audio_config.jitter_target_ms = 60;
  audio_config.jitter_max_ms = 120;
  video_config.payload_type = MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE;
  audio = mezon_audio_create(&audio_config);
  video = mezon_video_create(&video_config);
  if (!audio || !video) {
    fprintf(stderr, "failed to create media contexts\n");
    return 1;
  }
  printf("mezia core ready: Opus 48 kHz mono, 20 ms, 24 kbit/s, PT=%u\n",
         audio_config.payload_type);
  printf("H.264 hardware-codec payload type=%u; UDP/RTP is unencrypted.\n",
         video_config.payload_type);
  printf("Submit 960 PCM samples and call audio playout every 20 ms.\n");
  mezon_video_destroy(video);
  mezon_audio_destroy(audio);
  return 0;
}
