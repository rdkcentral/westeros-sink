#ifndef ISMD_VIDPPROC_H
#define ISMD_VIDPPROC_H

#ifdef NATIVE_BUILD

#include "ismd_core.h"

/*
 * Stub header for Intel CE ismd_vidpproc.h
 * Video post-processor module definitions
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

/* Video post-processor handle */
typedef ismd_dev_t ismd_vidpproc_t;

/* Video processing modes */
typedef enum {
    ISMD_VIDPPROC_MODE_PASSTHROUGH = 0,
    ISMD_VIDPPROC_MODE_PROCESS,
    ISMD_VIDPPROC_MODE_MAX
} ismd_vidpproc_mode_t;

/* Deinterlacing modes */
typedef enum {
    ISMD_VIDPPROC_DEINTERLACE_OFF = 0,
    ISMD_VIDPPROC_DEINTERLACE_BOB,
    ISMD_VIDPPROC_DEINTERLACE_WEAVE,
    ISMD_VIDPPROC_DEINTERLACE_MAX
} ismd_vidpproc_deinterlace_t;

/* Deinterlace policy modes */
typedef enum {
    ISMD_VIDPPROC_DI_POLICY_NONE = 0,
    ISMD_VIDPPROC_DI_POLICY_NEVER,
    ISMD_VIDPPROC_DI_POLICY_VIDEO,
    ISMD_VIDPPROC_DI_POLICY_MAX
} ismd_vidpproc_deinterlace_policy_t;

/* Video processing settings */
typedef struct {
    ismd_vidpproc_mode_t mode;
    ismd_vidpproc_deinterlace_t deinterlace;
    unsigned int width;
    unsigned int height;
} ismd_vidpproc_settings_t;

/* Function stubs */
static inline ismd_result_t ismd_vidpproc_open(ismd_vidpproc_t *vidpproc) {
    if (vidpproc) *vidpproc = (ismd_vidpproc_t)2;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidpproc_close(ismd_vidpproc_t vidpproc) {
    (void)vidpproc;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidpproc_set_settings(ismd_vidpproc_t vidpproc, ismd_vidpproc_settings_t *settings) {
    (void)vidpproc;
    (void)settings;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidpproc_get_settings(ismd_vidpproc_t vidpproc, ismd_vidpproc_settings_t *settings) {
    (void)vidpproc;
    if (settings) {
        settings->mode = ISMD_VIDPPROC_MODE_PASSTHROUGH;
        settings->deinterlace = ISMD_VIDPPROC_DEINTERLACE_OFF;
    }
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidpproc_set_deinterlace_policy(ismd_vidpproc_t vidpproc,
                                                                 ismd_vidpproc_deinterlace_policy_t policy) {
    (void)vidpproc;
    (void)policy;
    return ISMD_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* ISMD_VIDPPROC_H */
