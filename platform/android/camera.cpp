#include "platform_android.h"

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <stdio.h>
#include <string.h>

static void on_image(void *context, AImageReader *reader) {
  mezia_platform_t *p = (mezia_platform_t *)context;
  AImage *image = NULL;
  if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image) {
    return;
  }
  mezia_android_encoder_push_image(p, image);
  AImage_delete(image);
}

static void on_disconnected(void *context, ACameraDevice *device) {
  mezia_platform_t *p = (mezia_platform_t *)context;
  (void)device;
  mezia_platform_report_error(p, MEZON_ERR_STATE, "camera.disconnected");
}

static void on_error(void *context, ACameraDevice *device, int error) {
  mezia_platform_t *p = (mezia_platform_t *)context;
  (void)device;
  (void)error;
  mezia_platform_report_error(p, MEZON_ERR_INTERNAL, "camera.error");
}

static int pick_camera(ACameraManager *mgr, mezia_camera_facing_t facing,
                       char *out, size_t out_len) {
  ACameraIdList *ids = NULL;
  uint32_t i;
  int wanted = facing == MEZIA_CAMERA_BACK ? ACAMERA_LENS_FACING_BACK
                                           : ACAMERA_LENS_FACING_FRONT;
  if (ACameraManager_getCameraIdList(mgr, &ids) != ACAMERA_OK || !ids) {
    return 0;
  }
  for (i = 0; i < (uint32_t)ids->numCameras; ++i) {
    ACameraMetadata *meta = NULL;
    ACameraMetadata_const_entry entry;
    if (ACameraManager_getCameraCharacteristics(mgr, ids->cameraIds[i],
                                                &meta) != ACAMERA_OK) {
      continue;
    }
    if (ACameraMetadata_getConstEntry(meta, ACAMERA_LENS_FACING, &entry) ==
            ACAMERA_OK &&
        entry.data.u8[0] == (uint8_t)wanted) {
      snprintf(out, out_len, "%s", ids->cameraIds[i]);
      ACameraMetadata_free(meta);
      ACameraManager_deleteCameraIdList(ids);
      return 1;
    }
    ACameraMetadata_free(meta);
  }
  if (ids->numCameras > 0) {
    snprintf(out, out_len, "%s", ids->cameraIds[0]);
    ACameraManager_deleteCameraIdList(ids);
    return 1;
  }
  ACameraManager_deleteCameraIdList(ids);
  return 0;
}

mezon_status_t mezia_android_camera_start(mezia_platform_t *platform) {
  ACameraManager *mgr;
  ACameraDevice_StateCallbacks device_cb;
  AImageReader_ImageListener listener;
  ANativeWindow *window = NULL;
  ACaptureSessionOutputContainer *outputs = NULL;
  ACaptureSessionOutput *output = NULL;
  ACameraOutputTarget *target = NULL;
  ACaptureRequest *request = NULL;
  ACameraCaptureSession_stateCallbacks session_cb;
  media_status_t mst;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  mgr = ACameraManager_create();
  if (!mgr) {
    return MEZON_ERR_INTERNAL;
  }
  platform->camera_manager = mgr;
  if (!pick_camera(mgr, platform->config.camera, platform->camera_id,
                   sizeof(platform->camera_id))) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "camera.none");
    return MEZON_ERR_INTERNAL;
  }
  memset(&device_cb, 0, sizeof(device_cb));
  device_cb.context = platform;
  device_cb.onDisconnected = on_disconnected;
  device_cb.onError = on_error;
  if (ACameraManager_openCamera(mgr, platform->camera_id, &device_cb,
                                (ACameraDevice **)&platform->camera_device) !=
      ACAMERA_OK) {
    return MEZON_ERR_INTERNAL;
  }
  mst = AImageReader_new((int32_t)platform->config.video_width,
                         (int32_t)platform->config.video_height,
                         AIMAGE_FORMAT_YUV_420_888, 4,
                         (AImageReader **)&platform->image_reader);
  if (mst != AMEDIA_OK) {
    return MEZON_ERR_INTERNAL;
  }
  memset(&listener, 0, sizeof(listener));
  listener.context = platform;
  listener.onImageAvailable = on_image;
  AImageReader_setImageListener((AImageReader *)platform->image_reader,
                                &listener);
  AImageReader_getWindow((AImageReader *)platform->image_reader, &window);
  ACaptureSessionOutputContainer_create(&outputs);
  ACaptureSessionOutput_create(window, &output);
  ACaptureSessionOutputContainer_add(outputs, output);
  ACameraOutputTarget_create(window, &target);
  ACameraDevice_createCaptureRequest((ACameraDevice *)platform->camera_device,
                                     TEMPLATE_RECORD, &request);
  ACaptureRequest_addTarget(request, target);
  {
    int32_t fps[2] = {(int32_t)platform->config.video_fps,
                      (int32_t)platform->config.video_fps};
    ACaptureRequest_setEntry_i32(request, ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2,
                                 fps);
  }
  memset(&session_cb, 0, sizeof(session_cb));
  if (ACameraDevice_createCaptureSession(
          (ACameraDevice *)platform->camera_device, outputs, &session_cb,
          (ACameraCaptureSession **)&platform->capture_session) != ACAMERA_OK) {
    ACaptureRequest_free(request);
    ACameraOutputTarget_free(target);
    ACaptureSessionOutput_free(output);
    ACaptureSessionOutputContainer_free(outputs);
    return MEZON_ERR_INTERNAL;
  }
  ACameraCaptureSession_setRepeatingRequest(
      (ACameraCaptureSession *)platform->capture_session, NULL, 1, &request,
      NULL);
  ACaptureRequest_free(request);
  ACameraOutputTarget_free(target);
  ACaptureSessionOutput_free(output);
  ACaptureSessionOutputContainer_free(outputs);
  platform->camera_facing = platform->config.camera;
  return MEZON_OK;
}

void mezia_android_camera_stop(mezia_platform_t *platform) {
  if (!platform) {
    return;
  }
  if (platform->capture_session) {
    ACameraCaptureSession_stopRepeating(
        (ACameraCaptureSession *)platform->capture_session);
    ACameraCaptureSession_close(
        (ACameraCaptureSession *)platform->capture_session);
    platform->capture_session = NULL;
  }
  if (platform->camera_device) {
    ACameraDevice_close((ACameraDevice *)platform->camera_device);
    platform->camera_device = NULL;
  }
  if (platform->image_reader) {
    AImageReader_delete((AImageReader *)platform->image_reader);
    platform->image_reader = NULL;
  }
  if (platform->camera_manager) {
    ACameraManager_delete((ACameraManager *)platform->camera_manager);
    platform->camera_manager = NULL;
  }
}

mezon_status_t mezia_android_camera_set_facing(mezia_platform_t *platform,
                                               mezia_camera_facing_t facing) {
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->config.camera = facing;
  if (!platform->running) {
    return MEZON_OK;
  }
  mezia_android_camera_stop(platform);
  return mezia_android_camera_start(platform);
}
