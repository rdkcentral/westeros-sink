/* Stub header for V4L2 resource management */
#ifndef RESOURCEMANAGE_H
#define RESOURCEMANAGE_H

#include <stdint.h>
#include <stdio.h>
#include <gst/video/video-color.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

/* Platform-specific constants */
#ifndef WST_MAX_PLANES
#define WST_MAX_PLANES (3)
#endif

/* Missing GStreamer video transfer constants for older versions */
#ifndef GST_VIDEO_TRANSFER_BT2020_10
#define GST_VIDEO_TRANSFER_BT2020_10 14
#endif
#ifndef GST_VIDEO_TRANSFER_SMPTE_ST_2084
#define GST_VIDEO_TRANSFER_SMPTE_ST_2084 16
#endif
#ifndef GST_VIDEO_TRANSFER_ARIB_STD_B67
#define GST_VIDEO_TRANSFER_ARIB_STD_B67 17
#endif

/* DRM ioctl and flag definitions for older libdrm versions */
#ifndef DRM_IOCTL_GEM_CLOSE
#define DRM_IOCTL_GEM_CLOSE DRM_IOW(0x40, 0x09, struct drm_gem_close)
#endif
#ifndef DRM_IOCTL_MESON_GEM_CREATE
#define DRM_IOCTL_MESON_GEM_CREATE _IOWR(DRM_IOCTL_BASE, 0xa0, struct drm_meson_gem_create)
#endif
#ifndef DRM_CLOEXEC
#define DRM_CLOEXEC 0x1
#endif
#ifndef DRM_RDWR
#define DRM_RDWR 0x2
#endif

/* DRM structure definitions for older libdrm versions */
#ifndef __drm_meson_gem_create_included
struct drm_meson_gem_create {
   __u64 size;
   __u32 flags;
   __u32 handle;
};
#define __drm_meson_gem_create_included
#endif

/* Meson video sink flags */
#ifndef MESON_USE_VIDEO_AFBC
#define MESON_USE_VIDEO_AFBC (1 << 0)
#endif
#ifndef MESON_USE_VIDEO_PLANE
#define MESON_USE_VIDEO_PLANE (1 << 1)
#endif
#ifndef MESON_USE_PROTECTED
#define MESON_USE_PROTECTED (1 << 2)
#endif

/* Resource manager constants - stub implementations */
#define RESMAN_APP_SEC_TVP 2
#define RESMAN_ID_SEC_TVP 0

/* Resource manager functions - stubs for compilation */
static inline int resman_support(void) { return 0; }
static inline int resman_resource_support(const char *res) { return 0; }
static inline int resman_init(const char *name, int app) { return -1; }
static inline int resman_acquire_para(int fd, int id, int timeout, int count, void *arg) { return -1; }
static inline void resman_release(int fd, int id) {}
static inline void resman_close(int fd) {}

#endif