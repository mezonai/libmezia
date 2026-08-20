#include "mezia/sdp.h"

#include <stdio.h>
#include <string.h>

void mezia_sdp_init(mezia_sdp_t *sdp) {
  if (sdp) {
    memset(sdp, 0, sizeof(*sdp));
  }
}

static void trim_cr(char *line) {
  size_t n = strlen(line);
  while (n && (line[n - 1] == '\r' || line[n - 1] == '\n' || line[n - 1] == ' ')) {
    line[--n] = '\0';
  }
}

static int copy_token(char *dst, size_t dst_size, const char *src) {
  size_t n;
  if (!dst || !src || dst_size == 0) {
    return -1;
  }
  n = strlen(src);
  if (n >= dst_size) {
    return -1;
  }
  memcpy(dst, src, n + 1);
  return 0;
}

static mezia_sdp_media_t *current_media(mezia_sdp_t *sdp, int section) {
  if (section == 1) {
    return &sdp->audio;
  }
  if (section == 2) {
    return &sdp->video;
  }
  return NULL;
}

static int parse_rtpmap(const char *value, mezia_sdp_media_t *media) {
  unsigned int pt = 0;
  char codec[MEZIA_SDP_MAX_CODEC];
  unsigned int rate = 0;
  unsigned int channels = 1;
  int n;
  codec[0] = '\0';
  n = sscanf(value, "%u %15[^/]/%u/%u", &pt, codec, &rate, &channels);
  if (n < 3 || pt > 127U) {
    return -1;
  }
  media->payload_type = (uint8_t)pt;
  if (copy_token(media->codec, sizeof(media->codec), codec) != 0) {
    return -1;
  }
  media->clock_rate = rate;
  media->channels = (uint8_t)(n == 4 ? channels : 1U);
  return 0;
}

static int parse_extmap(const char *value, mezia_sdp_media_t *media) {
  unsigned int id = 0;
  char uri[96];
  if (media->extmap_count >= MEZIA_SDP_MAX_EXTMAP) {
    return 0;
  }
  uri[0] = '\0';
  if (sscanf(value, "%u %95s", &id, uri) < 2 || id > 14U) {
    return -1;
  }
  media->extmap[media->extmap_count].id = (uint8_t)id;
  if (copy_token(media->extmap[media->extmap_count].uri,
                 sizeof(media->extmap[0].uri), uri) != 0) {
    return -1;
  }
  media->extmap_count++;
  return 0;
}

static int parse_bundle(mezia_sdp_t *sdp, const char *value) {
  const char *p = value;
  sdp->bundle = 1;
  sdp->bundle_mid_count = 0;
  while (*p && sdp->bundle_mid_count < MEZIA_SDP_MAX_MIDS) {
    char mid[MEZIA_SDP_MAX_MID];
    size_t i = 0;
    while (*p == ' ') {
      p++;
    }
    if (!*p) {
      break;
    }
    while (*p && *p != ' ' && i + 1 < sizeof(mid)) {
      mid[i++] = *p++;
    }
    mid[i] = '\0';
    if (copy_token(sdp->bundle_mids[sdp->bundle_mid_count], MEZIA_SDP_MAX_MID,
                   mid) != 0) {
      return -1;
    }
    sdp->bundle_mid_count++;
    while (*p && *p != ' ') {
      p++;
    }
  }
  return 0;
}

mezon_status_t mezia_sdp_parse(const char *text, mezia_sdp_t *sdp) {
  char line[512];
  const char *cursor;
  int section = 0;

  if (!text || !sdp) {
    return MEZON_ERR_INVALID_ARG;
  }
  mezia_sdp_init(sdp);
  cursor = text;
  while (*cursor) {
    size_t i = 0;
    const char *value;
    while (*cursor && *cursor != '\n' && i + 1 < sizeof(line)) {
      line[i++] = *cursor++;
    }
    line[i] = '\0';
    if (*cursor == '\n') {
      cursor++;
    }
    trim_cr(line);
    if (line[0] == '\0' || line[1] != '=') {
      continue;
    }
    value = line + 2;
    if (line[0] == 'm') {
      if (strncmp(value, "audio ", 6) == 0) {
        section = 1;
        sdp->audio.present = 1;
        sdp->audio.direction = MEZIA_SDP_DIR_SENDRECV;
      } else if (strncmp(value, "video ", 6) == 0) {
        section = 2;
        sdp->video.present = 1;
        sdp->video.direction = MEZIA_SDP_DIR_SENDRECV;
      } else {
        section = 0;
      }
      continue;
    }
    if (line[0] != 'a') {
      continue;
    }
    if (strncmp(value, "group:BUNDLE", 12) == 0) {
      if (parse_bundle(sdp, value + 12) != 0) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
    } else if (strncmp(value, "ice-ufrag:", 10) == 0) {
      if (copy_token(sdp->ice_ufrag, sizeof(sdp->ice_ufrag), value + 10) != 0) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
    } else if (strncmp(value, "ice-pwd:", 8) == 0) {
      if (copy_token(sdp->ice_pwd, sizeof(sdp->ice_pwd), value + 8) != 0) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
    } else if (strncmp(value, "fingerprint:", 12) == 0) {
      const char *sp = strchr(value + 12, ' ');
      if (!sp) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
      if ((size_t)(sp - (value + 12)) >= sizeof(sdp->fingerprint_algo)) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
      memcpy(sdp->fingerprint_algo, value + 12, (size_t)(sp - (value + 12)));
      sdp->fingerprint_algo[sp - (value + 12)] = '\0';
      if (copy_token(sdp->fingerprint, sizeof(sdp->fingerprint), sp + 1) != 0) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
    } else if (strncmp(value, "ice-options:", 12) == 0) {
      if (strstr(value + 12, "trickle")) {
        sdp->trickle = 1;
      }
    } else if (strncmp(value, "setup:", 6) == 0) {
      if (copy_token(sdp->setup, sizeof(sdp->setup), value + 6) != 0) {
        return MEZON_ERR_MALFORMED_PACKET;
      }
    } else {
      mezia_sdp_media_t *media = current_media(sdp, section);
      if (!media) {
        continue;
      }
      if (strncmp(value, "mid:", 4) == 0) {
        if (copy_token(media->mid, sizeof(media->mid), value + 4) != 0) {
          return MEZON_ERR_MALFORMED_PACKET;
        }
      } else if (strcmp(value, "sendrecv") == 0) {
        media->direction = MEZIA_SDP_DIR_SENDRECV;
      } else if (strcmp(value, "sendonly") == 0) {
        media->direction = MEZIA_SDP_DIR_SENDONLY;
      } else if (strcmp(value, "recvonly") == 0) {
        media->direction = MEZIA_SDP_DIR_RECVONLY;
      } else if (strcmp(value, "inactive") == 0) {
        media->direction = MEZIA_SDP_DIR_INACTIVE;
      } else if (strcmp(value, "rtcp-mux") == 0) {
        media->rtcp_mux = 1;
      } else if (strncmp(value, "rtpmap:", 7) == 0) {
        if (parse_rtpmap(value + 7, media) != 0) {
          return MEZON_ERR_MALFORMED_PACKET;
        }
      } else if (strncmp(value, "extmap:", 7) == 0) {
        if (parse_extmap(value + 7, media) != 0) {
          return MEZON_ERR_MALFORMED_PACKET;
        }
      } else if (strncmp(value, "rtcp-fb:", 8) == 0) {
        if (strstr(value + 8, "nack pli")) {
          media->pli = 1;
          media->nack = 1;
        } else if (strstr(value + 8, "nack")) {
          media->nack = 1;
        }
        if (strstr(value + 8, "transport-cc")) {
          media->transport_cc = 1;
        }
      }
    }
  }
  if (!sdp->audio.present && !sdp->video.present) {
    return MEZON_ERR_MALFORMED_PACKET;
  }
  return MEZON_OK;
}

static int append(char **cursor, size_t *remaining, const char *text) {
  size_t n = strlen(text);
  if (n + 1 > *remaining) {
    return -1;
  }
  memcpy(*cursor, text, n + 1);
  *cursor += n;
  *remaining -= n;
  return 0;
}

mezon_status_t mezia_sdp_write_offer(const mezia_sdp_offer_params_t *params,
                                     char *out, size_t out_capacity,
                                     size_t *out_len) {
  char line[1024];
  char *cursor;
  size_t remaining;
  uint8_t audio_pt;
  uint8_t video_pt;
  const char *audio_mid;
  const char *video_mid;
  const char *ufrag;
  const char *pwd;
  const char *algo;
  const char *fp;

  if (!params || !out || !out_len || out_capacity == 0) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (!params->offer_audio && !params->offer_video) {
    return MEZON_ERR_INVALID_ARG;
  }
  audio_pt = params->audio_payload_type ? params->audio_payload_type : 111U;
  video_pt = params->video_payload_type ? params->video_payload_type : 102U;
  audio_mid = params->audio_mid ? params->audio_mid : "0";
  video_mid = params->video_mid ? params->video_mid : "1";
  ufrag = params->ice_ufrag ? params->ice_ufrag : "mezia";
  pwd = params->ice_pwd ? params->ice_pwd : "meziapasswordmeziapassword";
  algo = params->fingerprint_algo ? params->fingerprint_algo : "sha-256";
  fp = params->fingerprint
           ? params->fingerprint
           : "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:"
             "00:00:00:00:00:00:00:00:00:00:00";

  cursor = out;
  remaining = out_capacity;
  out[0] = '\0';
  if (append(&cursor, &remaining,
             "v=0\r\n"
             "o=- 0 0 IN IP4 0.0.0.0\r\n"
             "s=-\r\n"
             "t=0 0\r\n") != 0) {
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  if (params->offer_audio && params->offer_video) {
    snprintf(line, sizeof(line), "a=group:BUNDLE %s %s\r\n", audio_mid,
             video_mid);
  } else if (params->offer_audio) {
    snprintf(line, sizeof(line), "a=group:BUNDLE %s\r\n", audio_mid);
  } else {
    snprintf(line, sizeof(line), "a=group:BUNDLE %s\r\n", video_mid);
  }
  if (append(&cursor, &remaining, line) != 0) {
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  snprintf(line, sizeof(line),
           "a=ice-ufrag:%s\r\n"
           "a=ice-pwd:%s\r\n"
           "a=fingerprint:%s %s\r\n"
           "a=ice-options:trickle\r\n"
           "a=setup:actpass\r\n",
           ufrag, pwd, algo, fp);
  if (append(&cursor, &remaining, line) != 0) {
    return MEZON_ERR_BUFFER_TOO_SMALL;
  }
  if (params->offer_audio) {
    snprintf(line, sizeof(line),
             "m=audio 9 UDP/TLS/RTP/SAVPF %u\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "a=mid:%s\r\n"
             "a=sendrecv\r\n"
             "a=rtcp-mux\r\n"
             "a=rtpmap:%u opus/48000/2\r\n"
             "a=fmtp:%u minptime=10;useinbandfec=1\r\n"
             "a=rtcp-fb:%u transport-cc\r\n"
             "a=extmap:6 "
             "http://www.ietf.org/id/"
             "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
             "a=extmap:7 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
             "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n",
             audio_pt, audio_mid, audio_pt, audio_pt, audio_pt);
    if (append(&cursor, &remaining, line) != 0) {
      return MEZON_ERR_BUFFER_TOO_SMALL;
    }
  }
  if (params->offer_video) {
    snprintf(line, sizeof(line),
             "m=video 9 UDP/TLS/RTP/SAVPF %u\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "a=mid:%s\r\n"
             "a=sendrecv\r\n"
             "a=rtcp-mux\r\n"
             "a=rtpmap:%u H264/90000\r\n"
             "a=fmtp:%u "
             "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id="
             "42e01f\r\n"
             "a=rtcp-fb:%u nack\r\n"
             "a=rtcp-fb:%u nack pli\r\n"
             "a=rtcp-fb:%u transport-cc\r\n"
             "a=extmap:6 "
             "http://www.ietf.org/id/"
             "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
             "a=extmap:7 urn:ietf:params:rtp-hdrext:sdes:mid\r\n",
             video_pt, video_mid, video_pt, video_pt, video_pt, video_pt,
             video_pt);
    if (append(&cursor, &remaining, line) != 0) {
      return MEZON_ERR_BUFFER_TOO_SMALL;
    }
  }
  *out_len = (size_t)(cursor - out);
  return MEZON_OK;
}
