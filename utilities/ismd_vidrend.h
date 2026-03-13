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
#ifndef ISMD_VIDREND_H
#define ISMD_VIDREND_H

#ifdef NATIVE_BUILD

#include "ismd_core.h"

/*
 * Stub header for Intel CE ismd_vidrend.h
 * Video renderer module definitions
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

/* Video renderer handle */
typedef ismd_dev_t ismd_vidrend_t;

/* Video renderer events */
typedef enum {
    ISMD_VIDREND_EVENT_UNDERFLOW = 0,
    ISMD_VIDREND_EVENT_FIRST_PTS,
    ISMD_VIDREND_EVENT_PTS_VALUE,
    ISMD_VIDREND_EVENT_LAST
} ismd_vidrend_event_t;

/* Video renderer mute modes */
typedef enum {
    ISMD_VIDREND_MUTE_NONE = 0,
    ISMD_VIDREND_MUTE_DISPLAY_BLACK_FRAME,
    ISMD_VIDREND_MUTE_MODE_MAX
} ismd_vidrend_mute_mode_t;

/* Video format structures */
typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int frame_rate;
} ismd_vidrend_format_t;

/* Function stubs */
static inline ismd_result_t ismd_vidrend_open(ismd_vidrend_t *vidrend) {
    if (vidrend) *vidrend = (ismd_vidrend_t)1;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_close(ismd_vidrend_t vidrend) {
    (void)vidrend;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_set_video_format(ismd_vidrend_t vidrend, ismd_vidrend_format_t *format) {
    (void)vidrend;
    (void)format;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_set_video_plane(ismd_vidrend_t vidrend, int plane) {
    (void)vidrend;
    (void)plane;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_get_underflow_event(ismd_vidrend_t vidrend, ismd_event_t *event) {
    (void)vidrend;
    if (event) *event = (ismd_event_t)2;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_enable_port_output(ismd_vidrend_t vidrend,
                                                            int port_index,
                                                            int enable,
                                                            ismd_port_handle_t *port) {
    (void)vidrend;
    (void)port_index;
    (void)enable;
    if (port) *port = (ismd_port_handle_t)1;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_disable_port_output(ismd_vidrend_t vidrend) {
    (void)vidrend;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_vidrend_mute(ismd_vidrend_t vidrend,
                                              ismd_vidrend_mute_mode_t mute_mode) {
    (void)vidrend;
    (void)mute_mode;
    return ISMD_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* ISMD_VIDREND_H */
