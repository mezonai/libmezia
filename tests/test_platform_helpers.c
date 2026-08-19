#include "../platform/common/audio_ring.h"
#include "../platform/common/nal_split.h"

#include <stdio.h>
#include <string.h>

static int g_nals;
static size_t g_last_len;
static int g_last_eoa;

static void visit(const uint8_t *nal, size_t nal_len, int eoa, void *user) {
  (void)user;
  g_nals++;
  g_last_len = nal_len;
  g_last_eoa = eoa;
  if (nal_len == 0 || (nal[0] & 0x80U)) {
    fprintf(stderr, "bad nal\n");
  }
}

int main(void) {
  mezia_audio_ring_t ring;
  int16_t in[960];
  int16_t out[960];
  uint8_t annexb[] = {0, 0, 0, 1, 0x67, 0x42, 0, 0, 1, 0x68, 0xce, 0, 0, 0, 1,
                      0x65, 0x88, 0x84};
  size_t i;

  mezia_audio_ring_init(&ring);
  for (i = 0; i < 960; ++i) {
    in[i] = (int16_t)i;
  }
  if (mezia_audio_ring_write(&ring, in, 960) != 960) {
    return 1;
  }
  if (mezia_audio_ring_read(&ring, out, 960) != 960) {
    return 2;
  }
  if (memcmp(in, out, sizeof(in)) != 0) {
    return 3;
  }

  g_nals = 0;
  mezia_split_annexb(annexb, sizeof(annexb), visit, NULL);
  if (g_nals != 3 || !g_last_eoa) {
    fprintf(stderr, "nals=%d eoa=%d last=%zu\n", g_nals, g_last_eoa, g_last_len);
    return 4;
  }
  if (mezia_rtp_video_timestamp(1000000000ULL) != 90000U) {
    return 5;
  }
  return 0;
}
