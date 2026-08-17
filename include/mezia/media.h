#ifndef mezia_MEDIA_H
#define mezia_MEDIA_H

#include "mezia/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezia_ctx mezia_ctx_t;

mezia_ctx_t *mezia_create(void);
void mezia_destroy(mezia_ctx_t *ctx);
mezon_status_t mezia_start(mezia_ctx_t *ctx);
mezon_status_t mezia_stop(mezia_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
