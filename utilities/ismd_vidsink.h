/*
* Copyright (C) 2016 RDK Management
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
#ifndef ISMD_VIDSINK_H
#define ISMD_VIDSINK_H

#ifdef NATIVE_BUILD

#include "ismd_core.h"
#include "gdl_types.h"

/*
 * Stub header for Intel CE ismd_vidsink.h
 * Video sink device module definitions
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

/* Video sink device handle */
typedef ismd_dev_t ismd_vidsink_dev_t;

/* Video sink states */
typedef enum {
    ISMD_DEV_STATE_INVALID = -1,
    ISMD_DEV_STATE_INIT = 0,
    ISMD_DEV_STATE_PAUSE,
    ISMD_DEV_STATE_PLAY,
    ISMD_DEV_STATE_MAX
} ismd_dev_state_t;

/* Video sink scaling policy */
typedef enum {
    ISMD_VIDPPROC_SCALING_POLICY_NO_SCALING = 0,
    ISMD_VIDPPROC_SCALING_POLICY_SCALE_TO_FIT,
    ISMD_VIDPPROC_SCALING_POLICY_ZOOM_TO_FIT,
    ISMD_VIDPPROC_SCALING_POLICY_ZOOM_TO_FILL,
    ISMD_VIDPPROC_SCALING_POLICY_PRESERVE_ASPECT_RATIO,
    ISMD_VIDPPROC_SCALING_POLICY_FULL_SCREEN,
    ISMD_VIDPPROC_SCALING_POLICY_MAX
} ismd_vidpproc_scaling_policy_t;

/* Video sink scaling parameters */
typedef struct {
    unsigned int output_window_x;
    unsigned int output_window_y;
    unsigned int output_window_width;
    unsigned int output_window_height;
    unsigned int crop_hoff;
    unsigned int crop_voff;
    unsigned int crop_width;
    unsigned int crop_height;
    ismd_vidpproc_scaling_policy_t scaling_policy;
    int crop_enable;
    gdl_rectangle_t crop_window;
    gdl_rectangle_t dest_window;
    gdl_aspect_ratio_t aspect_ratio;
} ismd_vidsink_scale_params_t;

/* Function stubs */
static inline ismd_result_t ismd_vidsink_open(ismd_vidsink_dev_t *vidsink) {
    if (vidsink) *vidsink = (ismd_vidsink_dev_t)3;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_close(ismd_vidsink_dev_t vidsink) {
    (void)vidsink;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_smd_handles(ismd_vidsink_dev_t vidsink, 
                                                        ismd_dev_t vidpproc, 
                                                        ismd_dev_t vidrend) {
    (void)vidsink;
    (void)vidpproc;
    (void)vidrend;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_get_input_port(ismd_vidsink_dev_t vidsink,
                                                       ismd_port_handle_t *port) {
    (void)vidsink;
    if (port) *port = (ismd_port_handle_t)1;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_clock(ismd_vidsink_dev_t vidsink,
                                                   ismd_clock_t clock) {
    (void)vidsink;
    (void)clock;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_state(ismd_vidsink_dev_t vidsink,
                                                   ismd_dev_state_t state) {
    (void)vidsink;
    (void)state;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_base_time(ismd_vidsink_dev_t vidsink,
                                                       ismd_time_t base_time) {
    (void)vidsink;
    (void)base_time;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_flush(ismd_vidsink_dev_t vidsink) {
    (void)vidsink;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_scale_params(ismd_vidsink_dev_t vidsink,
                                                         ismd_vidsink_scale_params_t *params) {
    (void)vidsink;
    (void)params;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_get_scale_params(ismd_vidsink_dev_t vidsink,
                                                         ismd_vidsink_scale_params_t *params) {
    (void)vidsink;
    if (params) {
        params->output_window_x = 0;
        params->output_window_y = 0;
        params->output_window_width = 1920;
        params->output_window_height = 1080;
        params->crop_hoff = 0;
        params->crop_voff = 0;
        params->crop_width = 1920;
        params->crop_height = 1080;
        params->scaling_policy = ISMD_VIDPPROC_SCALING_POLICY_SCALE_TO_FIT;
        params->crop_enable = 0;
        params->crop_window.h_offset = 0;
        params->crop_window.v_offset = 0;
        params->crop_window.width = 1920;
        params->crop_window.height = 1080;
        params->dest_window.h_offset = 0;
        params->dest_window.v_offset = 0;
        params->dest_window.width = 1920;
        params->dest_window.height = 1080;
        params->aspect_ratio.numerator = 1;
        params->aspect_ratio.denominator = 1;
    }
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidsink_set_global_scaling_params(ismd_vidsink_dev_t vidsink,
                                                                   ismd_vidsink_scale_params_t params) {
    (void)vidsink;
    (void)params;
    return ISMD_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* ISMD_VIDSINK_H */
