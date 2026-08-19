#include "mezia/platform.h"
#include "platform_android.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static void release_window(mezia_platform_t *p) {
  ANativeWindow *window;
  if (!p || !p->native_window) {
    return;
  }
  window = (ANativeWindow *)p->native_window;
  ANativeWindow_release(window);
  mezia_platform_set_render_target(p, NULL);
}

JNIEXPORT jlong JNICALL
Java_ai_mezon_mezia_Platform_nativeCreate(JNIEnv *env, jclass clazz,
                                          jstring localIp, jint localPort,
                                          jstring remoteIp, jint remotePort,
                                          jint audioSsrc, jint videoSsrc,
                                          jboolean enableVideo) {
  mezia_platform_config_t cfg;
  const char *local;
  const char *remote;
  mezia_platform_t *p;
  (void)clazz;
  if (!localIp || !remoteIp) {
    return 0;
  }
  memset(&cfg, 0, sizeof(cfg));
  local = env->GetStringUTFChars(localIp, NULL);
  remote = env->GetStringUTFChars(remoteIp, NULL);
  cfg.peer.local_ip = local;
  cfg.peer.local_port = (uint16_t)localPort;
  cfg.peer.remote_ip = remote;
  cfg.peer.remote_port = (uint16_t)remotePort;
  cfg.peer.mtu = MEZON_DEFAULT_MTU;
  cfg.audio_ssrc = (uint32_t)audioSsrc;
  cfg.video_ssrc = (uint32_t)videoSsrc;
  cfg.enable_audio = 1;
  cfg.enable_video = enableVideo ? 1 : 0;
  cfg.adaptation_enabled = 1;
  cfg.camera = MEZIA_CAMERA_FRONT;
  cfg.video_width = 1280;
  cfg.video_height = 720;
  cfg.video_fps = 30;
  cfg.video_bitrate_bps = 1500000;
  p = mezia_platform_create(&cfg);
  env->ReleaseStringUTFChars(localIp, local);
  env->ReleaseStringUTFChars(remoteIp, remote);
  return (jlong)(intptr_t)p;
}

JNIEXPORT jint JNICALL Java_ai_mezon_mezia_Platform_nativeStart(JNIEnv *env,
                                                                jclass clazz,
                                                                jlong handle) {
  (void)env;
  (void)clazz;
  return (jint)mezia_platform_start((mezia_platform_t *)(intptr_t)handle);
}

JNIEXPORT jint JNICALL Java_ai_mezon_mezia_Platform_nativeStop(JNIEnv *env,
                                                               jclass clazz,
                                                               jlong handle) {
  (void)env;
  (void)clazz;
  return (jint)mezia_platform_stop((mezia_platform_t *)(intptr_t)handle);
}

JNIEXPORT void JNICALL Java_ai_mezon_mezia_Platform_nativeDestroy(JNIEnv *env,
                                                                  jclass clazz,
                                                                  jlong handle) {
  mezia_platform_t *p = (mezia_platform_t *)(intptr_t)handle;
  (void)env;
  (void)clazz;
  if (!p) {
    return;
  }
  release_window(p);
  mezia_platform_destroy(p);
}

JNIEXPORT void JNICALL Java_ai_mezon_mezia_Platform_nativeSetWindow(
    JNIEnv *env, jclass clazz, jlong handle, jobject surface) {
  mezia_platform_t *p = (mezia_platform_t *)(intptr_t)handle;
  ANativeWindow *window = NULL;
  (void)clazz;
  if (!p) {
    return;
  }
  release_window(p);
  if (surface) {
    window = ANativeWindow_fromSurface(env, surface);
  }
  mezia_platform_set_render_target(p, window);
}

JNIEXPORT jint JNICALL Java_ai_mezon_mezia_Platform_nativeSetCamera(
    JNIEnv *env, jclass clazz, jlong handle, jint facing) {
  (void)env;
  (void)clazz;
  return (jint)mezia_platform_set_camera((mezia_platform_t *)(intptr_t)handle,
                                         facing ? MEZIA_CAMERA_BACK
                                                : MEZIA_CAMERA_FRONT);
}

JNIEXPORT jstring JNICALL Java_ai_mezon_mezia_Platform_nativeStats(JNIEnv *env,
                                                                   jclass clazz,
                                                                   jlong handle) {
  mezon_stats_t stats;
  char line[256];
  (void)clazz;
  memset(&stats, 0, sizeof(stats));
  mezia_platform_get_stats((mezia_platform_t *)(intptr_t)handle, &stats);
  snprintf(line, sizeof(line),
           "tx %llu/%llu  rx %llu/%llu  enc %llu dec %llu fec %llu plc %llu  "
           "kbps %u loss %u%%",
           (unsigned long long)stats.packets_sent,
           (unsigned long long)stats.bytes_sent,
           (unsigned long long)stats.packets_received,
           (unsigned long long)stats.bytes_received,
           (unsigned long long)stats.audio_frames_encoded,
           (unsigned long long)stats.audio_frames_decoded,
           (unsigned long long)stats.audio_frames_fec_recovered,
           (unsigned long long)stats.audio_frames_plc,
           stats.audio_current_bitrate_bps / 1000U,
           stats.audio_current_packet_loss_percent);
  return env->NewStringUTF(line);
}

#ifdef __cplusplus
}
#endif
