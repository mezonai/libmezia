#include "mezia/media.h"

struct mezia_ctx {
  int running;
};

mezia_ctx_t *mezia_create(void) { return NULL; }

void mezia_destroy(mezia_ctx_t *ctx) { (void)ctx; }

mezon_status_t mezia_start(mezia_ctx_t *ctx) {
  (void)ctx;
  return MEZON_ERR_NOT_READY;
}

mezon_status_t mezia_stop(mezia_ctx_t *ctx) {
  (void)ctx;
  return MEZON_ERR_NOT_READY;
}
