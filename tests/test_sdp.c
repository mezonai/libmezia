#include "mezia/sdp.h"

#include <assert.h>
#include <string.h>

int main(void) {
  const char *offer =
      "v=0\r\n"
      "o=- 1 1 IN IP4 0.0.0.0\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0 1\r\n"
      "a=ice-ufrag:ufrag1\r\n"
      "a=ice-pwd:passwordpasswordpassword\r\n"
      "a=fingerprint:sha-256 11:22:33:44\r\n"
      "a=ice-options:trickle\r\n"
      "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
      "a=mid:0\r\n"
      "a=sendrecv\r\n"
      "a=rtcp-mux\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=rtcp-fb:111 transport-cc\r\n"
      "a=extmap:1 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
      "m=video 9 UDP/TLS/RTP/SAVPF 102\r\n"
      "a=mid:1\r\n"
      "a=sendrecv\r\n"
      "a=rtcp-mux\r\n"
      "a=rtpmap:102 H264/90000\r\n";
  mezia_sdp_t sdp;
  mezia_sdp_offer_params_t params;
  char generated[2048];
  size_t generated_len = 0;
  mezia_sdp_t roundtrip;

  assert(mezia_sdp_parse(offer, &sdp) == MEZON_OK);
  assert(sdp.bundle);
  assert(sdp.trickle);
  assert(strcmp(sdp.ice_ufrag, "ufrag1") == 0);
  assert(strcmp(sdp.fingerprint_algo, "sha-256") == 0);
  assert(sdp.audio.present);
  assert(sdp.audio.payload_type == 111);
  assert(sdp.audio.clock_rate == 48000);
  assert(sdp.audio.channels == 2);
  assert(sdp.audio.rtcp_mux);
  assert(sdp.audio.transport_cc);
  assert(strcmp(sdp.audio.codec, "opus") == 0);
  assert(sdp.audio.extmap_count == 1);
  assert(sdp.video.present);
  assert(sdp.video.payload_type == 102);
  assert(strcmp(sdp.video.codec, "H264") == 0);

  memset(&params, 0, sizeof(params));
  params.ice_ufrag = "abc";
  params.ice_pwd = "defdefdefdefdefdefdefdef";
  params.fingerprint_algo = "sha-256";
  params.fingerprint = "AA:BB";
  params.offer_audio = 1;
  params.offer_video = 1;
  assert(mezia_sdp_write_offer(&params, generated, sizeof(generated),
                               &generated_len) == MEZON_OK);
  assert(generated_len > 0);
  assert(mezia_sdp_parse(generated, &roundtrip) == MEZON_OK);
  assert(roundtrip.audio.payload_type == 111);
  assert(roundtrip.video.payload_type == 102);
  assert(roundtrip.bundle);
  assert(roundtrip.audio.rtcp_mux);
  assert(roundtrip.video.rtcp_mux);
  assert(roundtrip.video.nack && roundtrip.video.pli);
  assert(strcmp(roundtrip.setup, "actpass") == 0);
  return 0;
}
