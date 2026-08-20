#include "mezia/session.h"

#include <assert.h>
#include <string.h>

int main(void) {
  mezia_session_config_t config;
  mezia_session_t *session;
  char offer[2048];
  size_t offer_len = 0;
  const mezia_sdp_t *local;
  const mezia_sdp_t *remote;

  memset(&config, 0, sizeof(config));
  config.local_ip = "127.0.0.1";
  config.local_port = 0;
  config.remote_ip = "127.0.0.1";
  config.remote_port = 9;
  config.offer_audio = 1;
  config.offer_video = 1;
  session = mezia_session_create(&config);
  assert(session);

  assert(mezia_session_start(session) == MEZON_ERR_NOT_READY);
  assert(mezia_session_create_offer(session, offer, sizeof(offer), &offer_len) ==
         MEZON_OK);
  assert(mezia_session_set_local_description(session, offer) == MEZON_OK);
  assert(mezia_session_set_remote_description(session, offer) == MEZON_OK);

  local = mezia_session_local_sdp(session);
  remote = mezia_session_remote_sdp(session);
  assert(local && remote);
  assert(local->audio.payload_type == 111);
  assert(remote->video.payload_type == 102);
  assert(local->bundle && local->audio.rtcp_mux);

  mezia_session_close(session);
  return 0;
}
