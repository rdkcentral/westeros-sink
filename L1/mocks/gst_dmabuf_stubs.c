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
