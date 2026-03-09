/*
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
 * RPI and platform-specific stubs for linking westeros-sink binaries
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mock khrn_platform_get_wl_display
void* khrn_platform_get_wl_display(void) {
    // Mock: return NULL
    return NULL;
}

// Mock OMX_UseEGLImage
int OMX_UseEGLImage(void *hComponent, void *pAppPrivate, unsigned int nPortIndex, void *pEglImage) {
    // Mock: return error code
    return -1;
}

// Mock wl_egl_window_create
void* wl_egl_window_create(void *surface, int width, int height) {
    // Mock: return NULL
    return NULL;
}

// Mock wl_egl_window_destroy
void wl_egl_window_destroy(void *egl_window) {
    // Mock: do nothing
}

// Mock wl_egl_window_get_attached_size
void wl_egl_window_get_attached_size(void *egl_window, int *width, int *height) {
    // Mock: set default size
    if (width) *width = 1280;
    if (height) *height = 720;
}

// Mock wl_egl_window_resize
void wl_egl_window_resize(void *egl_window, int width, int height, int dx, int dy) {
    // Mock: do nothing
}

#ifdef __cplusplus
}
#endif
