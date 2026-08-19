#ifndef MEZIA_NAL_SPLIT_H
#define MEZIA_NAL_SPLIT_H

#include "mezia/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mezia_nal_visitor_t)(const uint8_t *nal, size_t nal_len,
                                    int end_of_access_unit, void *user_data);

static inline size_t mezia_start_code_len(const uint8_t *data, size_t len,
                                          size_t offset) {
  if (offset + 3U <= len && data[offset] == 0 && data[offset + 1U] == 0 &&
      data[offset + 2U] == 1) {
    return 3U;
  }
  if (offset + 4U <= len && data[offset] == 0 && data[offset + 1U] == 0 &&
      data[offset + 2U] == 0 && data[offset + 3U] == 1) {
    return 4U;
  }
  return 0;
}

static inline void mezia_split_annexb(const uint8_t *data, size_t len,
                                      mezia_nal_visitor_t visit, void *user) {
  size_t i = 0;
  size_t start = 0;
  int have = 0;
  while (i < len) {
    size_t sc = mezia_start_code_len(data, len, i);
    if (sc) {
      if (have && visit) {
        visit(data + start, i - start, 0, user);
      }
      i += sc;
      start = i;
      have = 1;
    } else {
      ++i;
    }
  }
  if (have && start < len && visit) {
    visit(data + start, len - start, 1, user);
  } else if (!have && len && visit) {
    visit(data, len, 1, user);
  }
}

static inline uint32_t mezia_rtp_video_timestamp(uint64_t now_ns) {
  return (uint32_t)((now_ns * 90000ULL) / 1000000000ULL);
}

#ifdef __cplusplus
}
#endif

#endif
