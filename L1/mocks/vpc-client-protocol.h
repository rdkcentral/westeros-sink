#ifndef WESTEROS_SINK_L1_VPC_CLIENT_PROTOCOL_H
#define WESTEROS_SINK_L1_VPC_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <wayland-client.h>

struct wl_vpc;
struct wl_vpc_surface;
#define WL_VPC_SURFACE_PATHWAY_GRAPHICS 1u

struct wl_vpc_surface_listener {
    void (*video_path_change)(void *, struct wl_vpc_surface *, uint32_t);
    void (*video_xform_change)(void *, struct wl_vpc_surface *, int32_t, int32_t,
                               uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
};

extern const struct wl_interface wl_vpc_interface;
struct wl_vpc_surface *wl_vpc_get_vpc_surface(struct wl_vpc *, struct wl_surface *);
int wl_vpc_surface_add_listener(struct wl_vpc_surface *, const struct wl_vpc_surface_listener *, void *);
void wl_vpc_surface_destroy(struct wl_vpc_surface *);
void wl_vpc_destroy(struct wl_vpc *);
void wl_vpc_surface_set_geometry(struct wl_vpc_surface *, int32_t, int32_t, int32_t, int32_t);

#endif