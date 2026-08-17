#include "mezon_media/media.h"

struct mezon_media_ctx {
  int running;
};

mezon_media_ctx_t *mezon_media_create(void) { return NULL; }

void mezon_media_destroy(mezon_media_ctx_t *ctx) { (void)ctx; }

mezon_status_t mezon_media_start(mezon_media_ctx_t *ctx) {
  (void)ctx;
  return MEZON_ERR_NOT_READY;
}

mezon_status_t mezon_media_stop(mezon_media_ctx_t *ctx) {
  (void)ctx;
  return MEZON_ERR_NOT_READY;
}
