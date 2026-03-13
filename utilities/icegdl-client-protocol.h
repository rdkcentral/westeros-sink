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
#ifndef ICEGDL_CLIENT_PROTOCOL_H
#define ICEGDL_CLIENT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "ismd_core.h"

/* Forward declarations */
struct wl_icegdl;
struct wl_icegdl_listener;

/* wl_icegdl interface definition */
struct wl_icegdl_interface {
    void (*plane)(void *data, struct wl_icegdl *wl_icegdl, uint32_t plane);
    void (*format)(void *data, struct wl_icegdl *wl_icegdl, uint32_t format);
};

/* wl_icegdl listener structure */
struct wl_icegdl_listener {
    void (*plane)(void *data, struct wl_icegdl *wl_icegdl, uint32_t plane);
    void (*format)(void *data, struct wl_icegdl *wl_icegdl, uint32_t format);
};

/* Static interface instance for wl_registry_bind */
static const struct wl_interface wl_icegdl_interface = {
    "wl_icegdl", 1,
    0, NULL,
    0, NULL
};

/* Stub functions */
static inline void
wl_icegdl_destroy(struct wl_icegdl *wl_icegdl)
{
    (void)wl_icegdl;
}

static inline int
wl_icegdl_add_listener(struct wl_icegdl *wl_icegdl,
                       const struct wl_icegdl_listener *listener,
                       void *data)
{
    (void)wl_icegdl;
    (void)listener;
    (void)data;
    return 0;
}

static inline struct wl_buffer *
wl_icegdl_create_planar_buffer(struct wl_icegdl *wl_icegdl,
                               ismd_buffer_handle_t ismdBuffer,
                               int windowX,
                               int windowY,
                               int windowWidth,
                               int windowHeight,
                               int frameStride)
{
    (void)wl_icegdl;
    (void)ismdBuffer;
    (void)windowX;
    (void)windowY;
    (void)windowWidth;
    (void)windowHeight;
    (void)frameStride;
    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
