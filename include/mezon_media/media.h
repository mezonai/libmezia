#ifndef MEZON_MEDIA_MEDIA_H
#define MEZON_MEDIA_MEDIA_H

#include "mezon_media/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mezon_media_ctx mezon_media_ctx_t;

mezon_media_ctx_t *mezon_media_create(void);
void mezon_media_destroy(mezon_media_ctx_t *ctx);
mezon_status_t mezon_media_start(mezon_media_ctx_t *ctx);
mezon_status_t mezon_media_stop(mezon_media_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
