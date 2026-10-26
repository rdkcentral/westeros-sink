#ifndef WESTEROS_SINK_L1_SIMPLESHELL_CLIENT_PROTOCOL_H
#define WESTEROS_SINK_L1_SIMPLESHELL_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <wayland-client.h>

struct wl_simple_shell;
struct wl_simple_shell_listener {
    void (*surface_id)(void *, struct wl_simple_shell *, struct wl_surface *, uint32_t);
    void (*surface_created)(void *, struct wl_simple_shell *, uint32_t, const char *);
    void (*surface_destroyed)(void *, struct wl_simple_shell *, uint32_t, const char *);
    void (*surface_status)(void *, struct wl_simple_shell *, uint32_t, const char *, uint32_t, int32_t, int32_t, int32_t, int32_t, wl_fixed_t, wl_fixed_t);
    void (*get_surfaces_done)(void *, struct wl_simple_shell *);
};

extern const struct wl_interface wl_simple_shell_interface;
int wl_simple_shell_add_listener(struct wl_simple_shell *, const struct wl_simple_shell_listener *, void *);
void wl_simple_shell_destroy(struct wl_simple_shell *);
void wl_simple_shell_set_name(struct wl_simple_shell *, uint32_t, const char *);
void wl_simple_shell_set_visible(struct wl_simple_shell *, uint32_t, int32_t);
void wl_simple_shell_set_geometry(struct wl_simple_shell *, uint32_t, int32_t, int32_t, int32_t, int32_t);
void wl_simple_shell_set_zorder(struct wl_simple_shell *, uint32_t, wl_fixed_t);
void wl_simple_shell_set_opacity(struct wl_simple_shell *, uint32_t, wl_fixed_t);
void wl_simple_shell_get_status(struct wl_simple_shell *, uint32_t);

#endif