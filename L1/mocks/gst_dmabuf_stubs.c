/**
 * GStreamer dmabuf memory stubs for linking westeros-sink binaries
 * These are mock implementations to satisfy linker requirements
 */

#include <glib.h>
#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mock gst_is_dmabuf_memory - check if memory is dmabuf
gboolean gst_is_dmabuf_memory(GstMemory *mem) {
    // Mock: always return FALSE for test/stub
    return FALSE;
}

// Mock gst_dmabuf_memory_get_fd - get file descriptor from dmabuf memory
gint gst_dmabuf_memory_get_fd(GstMemory *mem) {
    // Mock: return invalid fd (-1)
    return -1;
}

#ifdef __cplusplus
}
#endif
