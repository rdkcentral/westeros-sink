/*
 * ISMD Hardware Stubs for ICEGDL Testing
 * Provides mock implementations to avoid actual hardware access
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ISMD Handle Types */
typedef int ismd_dev_handle_t;
typedef int ismd_port_handle_t;
typedef int ismd_clock_handle_t;
typedef int ismd_event_handle_t;
typedef unsigned int ismd_vidrend_t;
typedef unsigned int ismd_vidpproc_t;
typedef unsigned int ismd_vidsink_t;

/* ISMD Result Type */
typedef enum {
    ISMD_SUCCESS = 0,
    ISMD_ERROR_INVALID_HANDLE = 1,
} ismd_result_t;

/* ISMD Device States */
typedef enum {
    ISMD_DEV_STATE_PLAY,
    ISMD_DEV_STATE_PAUSE,
    ISMD_DEV_STATE_STOP
} ismd_dev_state_t;

/* ISMD Result Constants */
#define ISMD_DEV_HANDLE_INVALID (-1)
#define ISMD_PORT_HANDLE_INVALID (-1)
#define ISMD_CLOCK_HANDLE_INVALID (-1)
#define ISMD_EVENT_HANDLE_INVALID (-1)
#define ISMD_NO_PTS (~0UL)

/* Mock handle counter */
static int mock_handle_counter = 100;

/* ==================== VIDREND Functions ==================== */
ismd_result_t ismd_vidrend_open(ismd_vidrend_t *vidrend) {
    if (vidrend) {
        *vidrend = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidrend_enable_port_output(ismd_vidrend_t vidrend, int port, int enable, ismd_port_handle_t *port_handle) {
    if (port_handle) {
        *port_handle = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidrend_disable_port_output(ismd_vidrend_t vidrend) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidrend_get_underflow_event(ismd_vidrend_t vidrend, ismd_event_handle_t *event) {
    if (event) {
        *event = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidrend_set_video_plane(ismd_vidrend_t vidrend, int plane) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidrend_mute(ismd_vidrend_t vidrend, int mute_video, int mute_audio) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_dev_close(ismd_vidrend_t device) {
    return ISMD_SUCCESS;
}

/* ==================== VIDPPROC Functions ==================== */
ismd_result_t ismd_vidpproc_open(ismd_vidpproc_t *vidpproc) {
    if (vidpproc) {
        *vidpproc = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

/* ==================== VIDSINK Functions ==================== */
ismd_result_t ismd_vidsink_open(ismd_vidsink_t *vidsink) {
    if (vidsink) {
        *vidsink = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_set_smd_handles(ismd_vidsink_t vidsink, ismd_vidpproc_t vidpproc, ismd_vidrend_t vidrend) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_get_input_port(ismd_vidsink_t vidsink, ismd_port_handle_t *port) {
    if (port) {
        *port = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_set_clock(ismd_vidsink_t vidsink, ismd_clock_handle_t clock) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_set_state(ismd_vidsink_t vidsink, ismd_dev_state_t state) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_flush(ismd_vidsink_t vidsink) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_close(ismd_vidsink_t vidsink) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_vidsink_set_base_time(ismd_vidsink_t vidsink, unsigned long base_time) {
    return ISMD_SUCCESS;
}

/* ==================== CLOCK Functions ==================== */
ismd_result_t ismd_clock_get_time(ismd_clock_handle_t clock, unsigned long *time_ptr) {
    if (time_ptr) {
        *time_ptr = 0;
    }
    return ISMD_SUCCESS;
}

/* ==================== EVENT Functions ==================== */
ismd_result_t ismd_event_alloc(ismd_event_handle_t *event) {
    if (event) {
        *event = mock_handle_counter++;
    }
    return ISMD_SUCCESS;
}

ismd_result_t ismd_event_set(ismd_event_handle_t event) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_event_free(ismd_event_handle_t event) {
    return ISMD_SUCCESS;
}

ismd_result_t ismd_event_wait_multiple(ismd_event_handle_t *events, int num_events, int timeout_ms, ismd_event_handle_t *triggered_event) {
    return ISMD_SUCCESS;
}

/* ==================== BUFFER Functions ==================== */
typedef struct {
    unsigned char *base;
    int size;
} ismd_buffer_descriptor_t;

ismd_result_t ismd_buffer_read_desc(int buffer_handle, ismd_buffer_descriptor_t *desc) {
    if (desc) {
        desc->base = NULL;
        desc->size = 0;
    }
    return ISMD_SUCCESS;
}

/* ==================== TAG Functions ==================== */
typedef struct {
    int rate_valid;
    int requested_rate;
    int applied_rate;
} ismd_newsegment_tag_t;

void ismd_tag_set_newsegment(int buffer, ismd_newsegment_tag_t tag) {
}

/* ==================== Utility Functions ==================== */
void ismd_gst_buffer_check_type_stub(void *buffer) {
}

int ismd_gst_buffer_get_handle_stub(void *buffer) {
    return 0;
}

void ismd_gst_register_device_handle_stub(int device, int device_type) {
}

void* ismd_gst_clock_get_default_clock_stub(void) {
    static int dummy_clock = 0;
    return &dummy_clock;
}

void ismd_gst_clock_mark_basetime_stub(void *clock) {
}

void ismd_gst_clock_clear_basetime_stub(void *clock) {
}

/* ==================== IceGDL Callback Functions ==================== */

void icegdlFormat(void *data, void *icegdl, uint32_t format) {
    (void)data;     
    (void)icegdl;   
    (void)format;   
}

void icegdlPlane(void *data, void *icegdl, uint32_t plane) {
    (void)data;     
    (void)icegdl;   
    (void)plane;    
}
