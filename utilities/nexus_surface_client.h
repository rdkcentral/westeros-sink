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
#ifndef NEXUS_SURFACE_CLIENT_H
#define NEXUS_SURFACE_CLIENT_H

#ifdef NATIVE_BUILD
/*
 * Stub header for Broadcom Nexus surface client
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Forward declarations */
typedef void* NEXUS_SurfaceClientHandle;
typedef void* NEXUS_SurfaceHandle;

/* Rect structure */
typedef struct NEXUS_Rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} NEXUS_Rect;

/* Video Window Content Mode */
typedef enum NEXUS_VideoWindowContentMode {
    NEXUS_VideoWindowContentMode_eBox = 0,
    NEXUS_VideoWindowContentMode_eFull,
    NEXUS_VideoWindowContentMode_eZoom,
    NEXUS_VideoWindowContentMode_ePanScan,
    NEXUS_VideoWindowContentMode_eMax
} NEXUS_VideoWindowContentMode;

/* Surface Composition */
typedef struct NEXUS_SurfaceComposition {
    NEXUS_Rect position;
    NEXUS_Rect clipRect;
    NEXUS_Rect virtualDisplay;
    bool visible;
    NEXUS_VideoWindowContentMode contentMode;
    uint32_t zorder;
} NEXUS_SurfaceComposition;

/* Surface Client Settings */
typedef struct NEXUS_SurfaceClientSettings {
    NEXUS_SurfaceComposition composition;
} NEXUS_SurfaceClientSettings;

/* Surface Client Status */
typedef struct NEXUS_SurfaceClientDisplayStatus {
    NEXUS_Rect framebuffer;
} NEXUS_SurfaceClientDisplayStatus;

typedef struct NEXUS_SurfaceClientStatus {
    NEXUS_SurfaceClientDisplayStatus display;
} NEXUS_SurfaceClientStatus;

/* Surface client functions */
static inline void NEXUS_SurfaceClient_GetDefaultSettings(NEXUS_SurfaceClientSettings *settings) { 
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SurfaceClientSettings));
    }
}

static inline int NEXUS_SurfaceClient_GetSettings(NEXUS_SurfaceClientHandle client, NEXUS_SurfaceClientSettings *settings) {
    (void)client;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SurfaceClientSettings));
    }
    return 0;
}

static inline int NEXUS_SurfaceClient_SetSettings(NEXUS_SurfaceClientHandle client, const NEXUS_SurfaceClientSettings *settings) {
    (void)client;
    (void)settings;
    return 0;
}

static inline int NEXUS_SurfaceClient_GetStatus(NEXUS_SurfaceClientHandle client, NEXUS_SurfaceClientStatus *status) {
    (void)client;
    if(status) {
        memset(status, 0, sizeof(NEXUS_SurfaceClientStatus));
        status->display.framebuffer.width = 1920;
        status->display.framebuffer.height = 1080;
    }
    return 0;
}

static inline NEXUS_SurfaceClientHandle NEXUS_SurfaceClient_Acquire(unsigned surfaceClientId) { 
    (void)surfaceClientId; 
    return (NEXUS_SurfaceClientHandle)1; 
}

static inline NEXUS_SurfaceClientHandle NEXUS_SurfaceClient_AcquireVideoWindow(NEXUS_SurfaceClientHandle client, unsigned windowId) {
    (void)client;
    (void)windowId;
    return (NEXUS_SurfaceClientHandle)1;
}

static inline void NEXUS_SurfaceClient_Release(NEXUS_SurfaceClientHandle handle) { 
    (void)handle; 
}

static inline void NEXUS_SurfaceClient_ReleaseVideoWindow(NEXUS_SurfaceClientHandle handle) {
    (void)handle;
}

#endif /* NATIVE_BUILD */

#endif /* NEXUS_SURFACE_CLIENT_H */
