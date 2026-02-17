/* Stub header for Raspberry Pi BCM Host */
#ifndef BCM_HOST_H
#define BCM_HOST_H

#ifdef NATIVE_BUILD

#include <stdint.h>

/* DISPMANX display IDs */
typedef enum {
    DISPMANX_ID_MAIN_LCD = 0,
    DISPMANX_ID_AUX_LCD = 1,
    DISPMANX_ID_HDMI = 2,
    DISPMANX_ID_SDTV = 3,
    DISPMANX_ID_FORCE_LCD = 0x100,
    DISPMANX_ID_FORCE_TV = 0x200,
    DISPMANX_ID_FORCE_OTHER = 0x300,
    DISPMANX_ID_COUNT = 4
} DISPMANX_ID_T;

/* DISPMANX resource handle types */
typedef uint32_t DISPMANX_DISPLAY_HANDLE_T;
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;
typedef uint32_t DISPMANX_UPDATE_HANDLE_T;
typedef uint32_t DISPMANX_ELEMENT_HANDLE_T;

/* Graphics API functions */
static inline void bcm_host_init(void) {}
static inline void bcm_host_deinit(void) {}

static inline int graphics_get_display_size(DISPMANX_ID_T display_id,
                                           uint32_t *width,
                                           uint32_t *height) {
    if (width) *width = 1920;
    if (height) *height = 1080;
    return 0;
}

/* DISPMANX API stubs */
static inline DISPMANX_DISPLAY_HANDLE_T vc_dispmanx_display_open(uint32_t device) {
    return 0;
}

static inline int vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T display) {
    return 0;
}

static inline DISPMANX_RESOURCE_HANDLE_T vc_dispmanx_resource_create(uint32_t type,
                                                                    uint32_t width,
                                                                    uint32_t height,
                                                                    uint32_t *native_image_handle) {
    return 0;
}

static inline int vc_dispmanx_resource_delete(DISPMANX_RESOURCE_HANDLE_T res) {
    return 0;
}

static inline DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update_start(int32_t priority) {
    return 0;
}

static inline int vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T update) {
    return 0;
}

static inline DISPMANX_ELEMENT_HANDLE_T vc_dispmanx_element_add(DISPMANX_UPDATE_HANDLE_T update,
                                                               DISPMANX_DISPLAY_HANDLE_T display,
                                                               int32_t layer,
                                                               void *dest_rect,
                                                               DISPMANX_RESOURCE_HANDLE_T src,
                                                               void *src_rect,
                                                               uint32_t protection,
                                                               void *alpha,
                                                               void *clamp,
                                                               uint32_t transform) {
    return 0;
}

static inline int vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T update,
                                            DISPMANX_ELEMENT_HANDLE_T element) {
    return 0;
}

#endif /* NATIVE_BUILD */

#endif /* BCM_HOST_H */