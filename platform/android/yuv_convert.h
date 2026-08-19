#ifndef MEZIA_YUV_CONVERT_H
#define MEZIA_YUV_CONVERT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pack Android YUV_420_888 (arbitrary UV stride) into NV12. */
static inline void mezia_yuv420_888_to_nv12(uint8_t *dst, size_t dst_stride,
                                            const uint8_t *y, size_t y_stride,
                                            const uint8_t *u, size_t u_stride,
                                            const uint8_t *v, size_t v_stride,
                                            int uv_pixel_stride, int width,
                                            int height) {
  int row;
  int col;
  uint8_t *dst_uv;
  for (row = 0; row < height; ++row) {
    memcpy(dst + (size_t)row * dst_stride, y + (size_t)row * y_stride,
           (size_t)width);
  }
  dst_uv = dst + (size_t)height * dst_stride;
  for (row = 0; row < height / 2; ++row) {
    uint8_t *out = dst_uv + (size_t)row * dst_stride;
    const uint8_t *urow = u + (size_t)row * u_stride;
    const uint8_t *vrow = v + (size_t)row * v_stride;
    for (col = 0; col < width / 2; ++col) {
      out[col * 2] = urow[col * uv_pixel_stride];
      out[col * 2 + 1] = vrow[col * uv_pixel_stride];
    }
  }
}

#ifdef __cplusplus
}
#endif

#endif
