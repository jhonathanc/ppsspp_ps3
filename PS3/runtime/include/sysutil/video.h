// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.
#pragma once

#if __has_include(<sysutil/video_out.h>)
#include <sysutil/video_out.h>

// ps3aqua renamed the video output API. Keep legacy frontends on its current ABI.
typedef videoOutResolution videoResolution;
typedef videoOutDisplayMode videoDisplayMode;
typedef videoOutState videoState;
typedef videoOutConfiguration videoConfiguration;
typedef videoOutDeviceInfo videoDeviceInfo;

#define videoGetState videoOutGetState
#define videoGetResolution videoOutGetResolution
#define videoConfigure videoOutConfigure
#define videoGetNumberOfDevice videoOutGetNumberOfDevice
#define videoGetDeviceInfo videoOutGetDeviceInfo
#define videoGetConfiguration videoOutGetConfiguration
#define videoGetResolutionAvailability videoOutGetResolutionAvailability

#define VIDEO_STATE_DISABLED VIDEO_OUT_STATE_DISABLED
#define VIDEO_STATE_ENABLED VIDEO_OUT_STATE_ENABLED
#define VIDEO_STATE_BUSY VIDEO_OUT_STATE_BUSY
#define VIDEO_PRIMARY VIDEO_OUT_PRIMARY
#define VIDEO_SECONDARY VIDEO_OUT_SECONDARY
#define VIDEO_SCANMODE_INTERLACE VIDEO_OUT_SCAN_MODE_INTERLACE
#define VIDEO_SCANMODE_PROGRESSIVE VIDEO_OUT_SCAN_MODE_PROGRESSIVE
#define VIDEO_BUFFER_FORMAT_XRGB VIDEO_OUT_BUFFER_FORMAT_XRGB
#define VIDEO_BUFFER_FORMAT_XBGR VIDEO_OUT_BUFFER_FORMAT_XBGR
#define VIDEO_BUFFER_FORMAT_FLOAT VIDEO_OUT_BUFFER_FORMAT_FLOAT
#define VIDEO_ASPECT_AUTO VIDEO_OUT_ASPECT_AUTO
#define VIDEO_ASPECT_4_3 VIDEO_OUT_ASPECT_4_3
#define VIDEO_ASPECT_16_9 VIDEO_OUT_ASPECT_16_9
#define VIDEO_RESOLUTION_1080 VIDEO_OUT_RESOLUTION_1080
#define VIDEO_RESOLUTION_720 VIDEO_OUT_RESOLUTION_720
#define VIDEO_RESOLUTION_480 VIDEO_OUT_RESOLUTION_480
#define VIDEO_RESOLUTION_576 VIDEO_OUT_RESOLUTION_576
#define VIDEO_RESOLUTION_1600x1080 VIDEO_OUT_RESOLUTION_1600x1080
#define VIDEO_RESOLUTION_1440x1080 VIDEO_OUT_RESOLUTION_1440x1080
#define VIDEO_RESOLUTION_1280x1080 VIDEO_OUT_RESOLUTION_1280x1080
#define VIDEO_RESOLUTION_960x1080 VIDEO_OUT_RESOLUTION_960x1080
#else
#include_next <sysutil/video.h>
#endif
