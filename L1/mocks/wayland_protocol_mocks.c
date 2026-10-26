#include <wayland-client.h>
#include "simpleshell-client-protocol.h"
#include "vpc-client-protocol.h"
#include "simplebuffer-client-protocol.h"

const struct wl_interface wl_simple_shell_interface = { "wl_simple_shell", 1, 0, 0, 0, 0 };
const struct wl_interface wl_vpc_interface = { "wl_vpc", 1, 0, 0, 0, 0 };
const struct wl_interface wl_sb_interface = { "wl_sb", 1, 0, 0, 0, 0 };

int wl_simple_shell_add_listener(struct wl_simple_shell *s, const struct wl_simple_shell_listener *l, void *d)
{ (void)s; (void)l; (void)d; return 0; }
void wl_simple_shell_destroy(struct wl_simple_shell *s) { (void)s; }
void wl_simple_shell_set_name(struct wl_simple_shell *s, uint32_t id, const char *n) { (void)s; (void)id; (void)n; }
void wl_simple_shell_set_visible(struct wl_simple_shell *s, uint32_t id, int32_t v) { (void)s; (void)id; (void)v; }
void wl_simple_shell_set_geometry(struct wl_simple_shell *s, uint32_t id, int32_t x, int32_t y, int32_t w, int32_t h)
{ (void)s; (void)id; (void)x; (void)y; (void)w; (void)h; }
void wl_simple_shell_set_zorder(struct wl_simple_shell *s, uint32_t id, wl_fixed_t z) { (void)s; (void)id; (void)z; }
void wl_simple_shell_set_opacity(struct wl_simple_shell *s, uint32_t id, wl_fixed_t o) { (void)s; (void)id; (void)o; }
void wl_simple_shell_get_status(struct wl_simple_shell *s, uint32_t id) { (void)s; (void)id; }
struct wl_vpc_surface *wl_vpc_get_vpc_surface(struct wl_vpc *v, struct wl_surface *s) { (void)v; (void)s; return 0; }
int wl_vpc_surface_add_listener(struct wl_vpc_surface *s, const struct wl_vpc_surface_listener *l, void *d)
{ (void)s; (void)l; (void)d; return 0; }
void wl_vpc_surface_destroy(struct wl_vpc_surface *s) { (void)s; }
void wl_vpc_destroy(struct wl_vpc *v) { (void)v; }
void wl_vpc_surface_set_geometry(struct wl_vpc_surface *s, int32_t x, int32_t y, int32_t w, int32_t h)
{ (void)s; (void)x; (void)y; (void)w; (void)h; }