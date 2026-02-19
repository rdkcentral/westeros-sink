#ifdef __cplusplus
extern "C" {
#endif

// Mock wl_proxy_add_listener to prevent segfaults in unit tests
int wl_proxy_add_listener(struct wl_proxy* proxy, void (**listener)(void), void* data) {
    // Mock: do nothing, return 0 (success)
    return 0;
}

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif

// Mock wl_proxy_set_queue to prevent segfaults in unit tests
void wl_proxy_set_queue(struct wl_proxy* proxy, struct wl_event_queue* queue) {
    // Mock: do nothing
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

// Mock wl_proxy_marshal_constructor_versioned to prevent segfaults in unit tests
void* wl_proxy_marshal_constructor_versioned(struct wl_proxy* proxy, uint32_t opcode, const struct wl_interface* interface, uint32_t version, ...) {
    // Mock: return NULL to indicate failure in test
    return NULL;
}

#ifdef __cplusplus
}
#endif
// Mock wl_proxy_destroy to prevent SIGABRT in unit tests
#ifdef __cplusplus
extern "C" {
#endif

void wl_proxy_destroy(struct wl_proxy* proxy) {
    // Mock: do nothing
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

// Mock wl_simple_shell_interface
const struct wl_interface wl_simple_shell_interface = {
    .name = "wl_simple_shell",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};

// Mock wl_vpc_interface
const struct wl_interface wl_vpc_interface = {
    .name = "wl_vpc",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};

// Mock wl_vpc_surface_interface
const struct wl_interface wl_vpc_surface_interface = {
    .name = "wl_vpc_surface",
    .version = 1,
    .method_count = 0,
    .methods = 0,
    .event_count = 0,
    .events = 0,
};