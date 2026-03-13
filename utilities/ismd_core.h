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
/* Stub header for Intel CE ISMD Core */
#ifndef ISMD_CORE_H
#define ISMD_CORE_H

#ifdef NATIVE_BUILD

#include <stdint.h>
#include "gdl_types.h"
#include <time.h>

/* Basic ISMD result type */
typedef int ismd_result_t;
typedef int ismd_dev_t;
typedef int ismd_buffer_handle_t;
typedef int ismd_port_handle_t;
typedef int ismd_clock_t;
typedef int ismd_event_t;
typedef long long ismd_time_t;

/* Invalid handles */
#define ISMD_DEV_HANDLE_INVALID -1
#define ISMD_PORT_HANDLE_INVALID -1
#define ISMD_CLOCK_HANDLE_INVALID -1
#define ISMD_EVENT_HANDLE_INVALID -1
#define ISMD_NO_PTS ((ismd_time_t)-1)

/* Success code */
#define ISMD_SUCCESS 0

/* Error codes */
typedef enum {
    ISMD_ERROR_SUCCESS = 0,
    ISMD_ERROR_NO_SPACE_AVAILABLE = -1,
    ISMD_ERROR_INVALID_HANDLE = -2,
    ISMD_ERROR_INVALID_PARAM = -3
} ismd_error_t;

/* Timeout constants */
#define ISMD_TIMEOUT_NONE 0
#define ISMD_TIMEOUT_INFINITE 0xFFFFFFFF

/* Buffer types */
typedef enum {
    ISMD_BUFFER_TYPE_VIDEO_FRAME = 0,
    ISMD_BUFFER_TYPE_AUDIO_FRAME,
    ISMD_BUFFER_TYPE_TAG,
    ISMD_BUFFER_TYPE_MAX
} ismd_buffer_type_t;

/* Event list for waiting on multiple events */
typedef ismd_event_t ismd_event_list_t[4];

/* Buffer descriptor */
typedef struct {
    unsigned int phys;
    void *virt;
    unsigned int size;
    unsigned int read_offset;
    unsigned int write_offset;
    int buffer_type;
    void *attributes;
} ismd_buffer_descriptor_t;

/* Newsegment tag */
typedef struct {
    int rate_valid;
    int requested_rate;
    int applied_rate;
    long long segment_start;
    long long segment_stop;
    long long segment_position;
    long long start;
    long long stop;
    long long linear_start;
} ismd_newsegment_tag_t;

/* Frame attributes */
typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    unsigned int pixel_format;
    long long local_pts;
    long long original_pts;
    unsigned int local_cont_rate;
    unsigned int cont_rate;
    unsigned int time_code;
    unsigned int fmd_cadence_type;
    unsigned int fmd_frame_index;
    unsigned int scanline_stride;
    gdl_size_t cont_size;
} ismd_frame_attributes_t;

/* ISMD core functions */
static inline ismd_result_t ismd_core_init(void) { return ISMD_SUCCESS; }
static inline ismd_result_t ismd_core_shutdown(void) { return ISMD_SUCCESS; }

/* Device functions */
static inline ismd_result_t ismd_dev_close(ismd_dev_t dev) {
    (void)dev;
    return ISMD_SUCCESS;
}

/* Buffer functions */
static inline ismd_result_t ismd_buffer_read_desc(ismd_buffer_handle_t buffer,
                                                  ismd_buffer_descriptor_t *desc) {
    (void)buffer;
    if (desc) {
        desc->phys = 0;
        desc->virt = NULL;
        desc->size = 0;
        desc->read_offset = 0;
        desc->write_offset = 0;
    }
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_buffer_free(ismd_buffer_handle_t buffer) {
    (void)buffer;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_buffer_add_reference(ismd_buffer_handle_t buffer) {
    (void)buffer;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_buffer_dereference(ismd_buffer_handle_t buffer) {
    (void)buffer;
    return ISMD_SUCCESS;
}

/* Tag functions */
static inline ismd_result_t ismd_tag_set_newsegment(ismd_buffer_handle_t buffer,
                                                    ismd_newsegment_tag_t newsegment) {
    (void)buffer;
    (void)newsegment;
    return ISMD_SUCCESS;
}

/* Event functions */
static inline ismd_result_t ismd_event_alloc(ismd_event_t *event) {
    if (event) *event = (ismd_event_t)1;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_free(ismd_event_t event) {
    (void)event;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_set(ismd_event_t event) {
    (void)event;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_clear(ismd_event_t event) {
    (void)event;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_reset(ismd_event_t event) {
    (void)event;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_acknowledge(ismd_event_t event) {
    (void)event;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_event_wait_multiple(ismd_event_list_t events,
                                                     int num_events,
                                                     unsigned int timeout_ms,
                                                     ismd_event_t *event) {
    (void)events;
    (void)num_events;
    (void)timeout_ms;
    if (event) *event = events[0];
    return ISMD_SUCCESS;
}

/* Clock functions */
static inline ismd_result_t ismd_clock_get_time(ismd_clock_t clock, ismd_time_t *time) {
    (void)clock;
    if (time) *time = 0;
    return ISMD_SUCCESS;
}

static inline ismd_result_t ismd_clock_set_time(ismd_clock_t clock, ismd_time_t time) {
    (void)clock;
    (void)time;
    return ISMD_SUCCESS;
}

/* Port functions */
static inline ismd_result_t ismd_port_write(ismd_port_handle_t port, ismd_buffer_handle_t buffer) {
    (void)port;
    (void)buffer;
    return ISMD_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* ISMD_CORE_H */
