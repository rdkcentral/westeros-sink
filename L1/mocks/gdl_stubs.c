/*
 * GDL Graphics Display Library Stubs
 * Provides mock implementations for display management
 */

#include <string.h>

/* GDL Return Types */
typedef enum {
    GDL_SUCCESS = 0,
    GDL_ERROR = 1
} gdl_ret_t;

/* GDL Display IDs */
typedef enum {
    GDL_DISPLAY_ID_0 = 0,
    GDL_DISPLAY_ID_1 = 1
} gdl_display_id_t;

/* GDL Plane IDs */
typedef enum {
    GDL_PLANE_ID_UNDEFINED = -1,
    GDL_PLANE_ID_UPP_A = 0,
    GDL_PLANE_ID_UPP_B = 1,
    GDL_PLANE_ID_UPP_C = 2
} gdl_plane_id_t;

/* GDL Port IDs */
typedef enum {
    GDL_PD_ID_INTTVENC_COMPONENT = 0,
    GDL_PD_ID_INTTVENC = 1
} gdl_port_id_t;

/* GDL Attribute IDs */
typedef enum {
    GDL_ATTR_ID_ENABLE = 0,
    GDL_ATTR_ID_CC_ENABLE = 1
} gdl_attr_id_t;

/* GDL Boolean Type */
typedef int gdl_boolean_t;
typedef enum {
    GDL_FALSE = 0,
    GDL_TRUE = 1
} gdl_bool_values_t;

/* GDL Rectangle */
typedef struct {
    int origin_x;
    int origin_y;
    int width;
    int height;
} gdl_rectangle_t;

/* GDL Display Info */
typedef struct {
    int width;
    int height;
    int refresh_rate;
    int port;
} gdl_display_info_t;

/* ==================== GDL Initialization ==================== */
gdl_ret_t gdl_init(int app_id) {
    return GDL_SUCCESS;
}

gdl_ret_t gdl_close(void) {
    return GDL_SUCCESS;
}

/* ==================== Display Functions ==================== */
gdl_ret_t gdl_get_display_info(gdl_display_id_t display_id, gdl_display_info_t *info) {
    if (info == NULL) {
        return GDL_ERROR;
    }
    
    // Return default display info
    info->width = 1920;
    info->height = 1080;
    info->refresh_rate = 60;
    info->port = 0;
    
    return GDL_SUCCESS;
}

/* ==================== Plane Functions ==================== */
gdl_ret_t gdl_plane_reset(gdl_plane_id_t plane) {
    if (plane == GDL_PLANE_ID_UNDEFINED) {
        return GDL_ERROR;
    }
    return GDL_SUCCESS;
}

gdl_ret_t gdl_plane_set_rect(gdl_plane_id_t plane, gdl_rectangle_t *src, gdl_rectangle_t *dst) {
    if (plane == GDL_PLANE_ID_UNDEFINED) {
        return GDL_ERROR;
    }
    return GDL_SUCCESS;
}

gdl_ret_t gdl_plane_set_active(gdl_plane_id_t plane, gdl_boolean_t active) {
    if (plane == GDL_PLANE_ID_UNDEFINED) {
        return GDL_ERROR;
    }
    return GDL_SUCCESS;
}

/* ==================== Port Attribute Functions ==================== */
gdl_ret_t gdl_port_set_attr(gdl_port_id_t port, gdl_attr_id_t attr, gdl_boolean_t *value) {
    return GDL_SUCCESS;
}

gdl_ret_t gdl_port_get_attr(gdl_port_id_t port, gdl_attr_id_t attr, gdl_boolean_t *value) {
    if (value) {
        *value = GDL_FALSE;
    }
    return GDL_SUCCESS;
}

/* ==================== Surface Functions ==================== */
typedef struct {
    int width;
    int height;
    int pitch;
    void *virtual_addr;
} gdl_surface_t;

gdl_ret_t gdl_surface_create(int width, int height, gdl_surface_t *surface) {
    if (surface == NULL) {
        return GDL_ERROR;
    }
    
    surface->width = width;
    surface->height = height;
    surface->pitch = width * 4;
    surface->virtual_addr = NULL;
    
    return GDL_SUCCESS;
}

gdl_ret_t gdl_surface_destroy(gdl_surface_t *surface) {
    return GDL_SUCCESS;
}

/* ==================== Scaling Functions ==================== */
gdl_ret_t gdl_plane_set_scaling(gdl_plane_id_t plane, int src_w, int src_h, int dst_w, int dst_h) {
    if (plane == GDL_PLANE_ID_UNDEFINED) {
        return GDL_ERROR;
    }
    return GDL_SUCCESS;
}
