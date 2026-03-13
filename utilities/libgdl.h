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
/* Stub header for Intel GDL */
#ifndef LIBGDL_H  
#define LIBGDL_H

#ifdef NATIVE_BUILD

#include "gdl_types.h"

/* GDL initialization and teardown functions */
static inline gdl_ret_t gdl_init(unsigned int flags) { 
    (void)flags;
    return GDL_SUCCESS; 
}
static inline gdl_ret_t gdl_close(void) { return GDL_SUCCESS; }

/* GDL display information functions */
static inline gdl_ret_t gdl_get_display_info(gdl_display_id_t id, gdl_display_info_t *info) {
    (void)id;
    if (info) {
        info->id = 0;
        info->width = 1920;
        info->height = 1080;
        info->refresh_rate = 60;
        info->tvmode.width = 1920;
        info->tvmode.height = 1080;
        info->pixel_clock = 148500;
    }
    return GDL_SUCCESS;
}

/* GDL plane configuration functions */
static inline gdl_ret_t gdl_plane_config_begin(gdl_plane_id_t plane) {
    (void)plane;
    return GDL_SUCCESS;
}

static inline gdl_ret_t gdl_plane_config_end(gdl_boolean_t apply) {
    (void)apply;
    return GDL_SUCCESS;
}

static inline gdl_ret_t gdl_plane_set_uint(unsigned int id, unsigned int value) {
    (void)id;
    (void)value;
    return GDL_SUCCESS;
}

static inline gdl_ret_t gdl_plane_set_attr(unsigned int id, gdl_rectangle_t *rect) {
    (void)id;
    (void)rect;
    return GDL_SUCCESS;
}

static inline gdl_ret_t gdl_plane_reset(gdl_plane_id_t plane) {
    (void)plane;
    return GDL_SUCCESS;
}

/* GDL port and closed caption functions */
static inline gdl_ret_t gdl_port_set_attr(unsigned int port_id, unsigned int attr_id, gdl_boolean_t *value) {
    (void)port_id;
    (void)attr_id;
    (void)value;
    return GDL_SUCCESS;
}

static inline gdl_ret_t gdl_closed_caption_source(gdl_plane_id_t plane) {
    (void)plane;
    return GDL_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* LIBGDL_H */
