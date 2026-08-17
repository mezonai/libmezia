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
  audio_config.sample_rate = 48000;
  audio_config.channels = 2;
  audio_config.payload_type = MEZON_DEFAULT_AUDIO_PAYLOAD_TYPE;
  video_config.payload_type = MEZON_DEFAULT_VIDEO_PAYLOAD_TYPE;
  audio = mezon_audio_create(&audio_config);
  video = mezon_video_create(&video_config);
  if (!audio || !video) {
    fprintf(stderr, "failed to create media contexts\n");
    return 1;
  }
  printf("mezia portable core ready: PCM L16 PT=%u, H.264 PT=%u\n",
         audio_config.payload_type, video_config.payload_type);
  printf("Run ctest for RTP, media packetization, and UDP loopback checks.\n");
  mezon_video_destroy(video);
  mezon_audio_destroy(audio);
  return 0;
}
