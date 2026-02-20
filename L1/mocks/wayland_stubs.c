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
#ifdef __cplusplus
extern "C" {
#endif

int wl_proxy_add_listener(struct wl_proxy* proxy, void (**listener)(void), void* data) {
    return 0;
}

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif

void wl_proxy_set_queue(struct wl_proxy* proxy, struct wl_event_queue* queue) {
}

#ifdef __cplusplus
}
#endif
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* wl_proxy_marshal_constructor_versioned(struct wl_proxy* proxy, uint32_t opcode, const struct wl_interface* interface, uint32_t version, ...) {
    return NULL;
}

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif

void wl_proxy_destroy(struct wl_proxy* proxy) {
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

void wl_simple_shell_set_visible(struct wl_simple_shell* shell, uint32_t surface_id, int32_t visible) {
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

void wl_simple_shell_set_geometry(struct wl_simple_shell* shell, uint32_t surface_id, int32_t x, int32_t y, int32_t width, int32_t height) {
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

int wl_display_flush(struct wl_display* display) {
    return 0;
}

#ifdef __cplusplus
}
#endif

struct wl_interface {
    const char *name;
    int version;
    int method_count;
    const void *methods;
    int event_count;
    const void *events;
};

const struct wl_interface wl_sb_interface = {
    .name = "wl_sb",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};

const struct wl_interface wl_simple_shell_interface = {
    .name = "wl_simple_shell",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};

const struct wl_interface wl_vpc_interface = {
    .name = "wl_vpc",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};

const struct wl_interface wl_vpc_surface_interface = {
    .name = "wl_vpc_surface",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};
