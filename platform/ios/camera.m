#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "platform_ios.h"

@interface MeziaCameraDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, assign) mezia_platform_t *platform;
@end

@implementation MeziaCameraDelegate
- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
  (void)output;
  (void)connection;
  if (!self.platform || !self.platform->running) {
    return;
  }
  mezia_ios_encoder_encode(self.platform, sampleBuffer);
}
@end


static AVCaptureDevice *find_camera(mezia_camera_facing_t facing) {
  AVCaptureDevicePosition position = facing == MEZIA_CAMERA_BACK
                                         ? AVCaptureDevicePositionBack
                                         : AVCaptureDevicePositionFront;
  NSArray<AVCaptureDeviceType> *types = @[
    AVCaptureDeviceTypeBuiltInWideAngleCamera
  ];
  AVCaptureDeviceDiscoverySession *session =
      [AVCaptureDeviceDiscoverySession
          discoverySessionWithDeviceTypes:types
                                mediaType:AVMediaTypeVideo
                                 position:position];
  if (session.devices.count > 0) {
    return session.devices.firstObject;
  }
  return [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
}

mezon_status_t mezia_ios_camera_start(mezia_platform_t *platform) {
  NSError *error = nil;
  AVCaptureDevice *device;
  AVCaptureDeviceInput *input;
  AVCaptureVideoDataOutput *output;
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->capture = [[AVCaptureSession alloc] init];
  if ([platform->capture canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
    platform->capture.sessionPreset = AVCaptureSessionPreset1280x720;
  } else {
    platform->capture.sessionPreset = AVCaptureSessionPreset640x480;
  }
  device = find_camera(platform->config.camera);
  if (!device) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "camera.device");
    return MEZON_ERR_INTERNAL;
  }
  input = [[AVCaptureDeviceInput alloc] initWithDevice:device error:&error];
  if (!input || ![platform->capture canAddInput:input]) {
    mezia_platform_report_error(platform, MEZON_ERR_INTERNAL, "camera.input");
    return MEZON_ERR_INTERNAL;
  }
  [platform->capture addInput:input];
  platform->camera_input = input;
  platform->camera_facing = platform->config.camera;

  output = [[AVCaptureVideoDataOutput alloc] init];
  output.alwaysDiscardsLateVideoFrames = YES;
  output.videoSettings = @{
    (id)kCVPixelBufferPixelFormatTypeKey :
        @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
  };
  {
    MeziaCameraDelegate *delegate = [[MeziaCameraDelegate alloc] init];
    delegate.platform = platform;
    platform->camera_delegate = delegate;
    [output setSampleBufferDelegate:delegate queue:platform->camera_queue];
  }
  if (![platform->capture canAddOutput:output]) {
    return MEZON_ERR_INTERNAL;
  }
  [platform->capture addOutput:output];
  platform->video_output = output;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    [platform->capture startRunning];
  });
  return MEZON_OK;
}

void mezia_ios_camera_stop(mezia_platform_t *platform) {
  if (!platform || !platform->capture) {
    return;
  }
  [platform->capture stopRunning];
  platform->capture = nil;
  platform->camera_input = nil;
  platform->video_output = nil;
  platform->camera_delegate = nil;
}

mezon_status_t mezia_ios_camera_set_facing(mezia_platform_t *platform,
                                           mezia_camera_facing_t facing) {
  if (!platform) {
    return MEZON_ERR_INVALID_ARG;
  }
  platform->config.camera = facing;
  if (!platform->running) {
    return MEZON_OK;
  }
  mezia_ios_camera_stop(platform);
  return mezia_ios_camera_start(platform);
}
