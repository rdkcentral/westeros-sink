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
/* Stub header for Broadcom NxClient */
#ifndef NXCLIENT_H
#define NXCLIENT_H

#ifdef NATIVE_BUILD

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Video Window Type */
typedef enum NxClient_VideoWindowType {
    NxClient_VideoWindowType_eMain = 0,
    NxClient_VideoWindowType_ePip,
    NxClient_VideoWindowType_eMax
} NxClient_VideoWindowType;

/* Video Window Capabilities */
typedef struct NxClient_VideoWindowCapabilities {
    NxClient_VideoWindowType type;
    uint32_t maxWidth;
    uint32_t maxHeight;
} NxClient_VideoWindowCapabilities;

/* Video Decoder Feeder Capabilities */
typedef struct NxClient_VideoDecoderFeederCapabilities {
    uint32_t colorDepth;
} NxClient_VideoDecoderFeederCapabilities;

/* Video Decoder Capabilities */
typedef struct NxClient_VideoDecoderCapabilities {
    bool secureVideo;
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t colorDepth;
    NxClient_VideoDecoderFeederCapabilities feeder;
} NxClient_VideoDecoderCapabilities;

/* Simple Video Decoder Info */
typedef struct NxClient_SimpleVideoDecoderInfo {
    uint32_t id;
    uint32_t surfaceClientId;
    uint32_t windowId;
    NxClient_VideoWindowCapabilities windowCapabilities;
    NxClient_VideoDecoderCapabilities decoderCapabilities;
} NxClient_SimpleVideoDecoderInfo;

/* Surface Client Info */
typedef struct NxClient_SurfaceClientInfo {
    uint32_t id;
} NxClient_SurfaceClientInfo;

/* Alloc Settings */
typedef struct NxClient_AllocSettings {
    uint32_t surfaceClient;
    uint32_t simpleVideoDecoder;
} NxClient_AllocSettings;

/* Alloc Results */
typedef struct NxClient_AllocResults {
    NxClient_SurfaceClientInfo surfaceClient[1];
    NxClient_SimpleVideoDecoderInfo simpleVideoDecoder[1];
} NxClient_AllocResults;

/* Connect Settings */
typedef struct NxClient_ConnectSettings {
    NxClient_SimpleVideoDecoderInfo simpleVideoDecoder[1];
} NxClient_ConnectSettings;

/* Display Settings */
typedef struct NxClient_HdmiPreferences {
    int dynamicRangeMode;
} NxClient_HdmiPreferences;

typedef struct NxClient_DisplaySettings {
    int format;
    NxClient_HdmiPreferences hdmiPreferences;
} NxClient_DisplaySettings;

/* NxClient functions */
static inline int NxClient_Join(void *settings) { 
    (void)settings; 
    return 0; 
}

static inline void NxClient_Uninit(void) {}

static inline void NxClient_GetDefaultAllocSettings(NxClient_AllocSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NxClient_AllocSettings));
    }
}

static inline int NxClient_Alloc(const NxClient_AllocSettings *settings, NxClient_AllocResults *results) {
    (void)settings;
    if(results) {
        memset(results, 0, sizeof(NxClient_AllocResults));
        results->surfaceClient[0].id = 1;
        results->simpleVideoDecoder[0].id = 1;
    }
    return 0;
}

static inline void NxClient_Free(const NxClient_AllocResults *results) {
    (void)results;
}

static inline void NxClient_GetDefaultConnectSettings(NxClient_ConnectSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NxClient_ConnectSettings));
    }
}

static inline int NxClient_Connect(const NxClient_ConnectSettings *settings, unsigned *connectId) {
    (void)settings;
    if(connectId) {
        *connectId = 1;
    }
    return 0;
}

static inline void NxClient_Disconnect(unsigned connectId) {
    (void)connectId;
}

static inline int NxClient_RefreshConnect(unsigned connectId) {
    (void)connectId;
    return 0;
}

static inline void NxClient_GetDisplaySettings(NxClient_DisplaySettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NxClient_DisplaySettings));
    }
}

static inline int NxClient_SetDisplaySettings(const NxClient_DisplaySettings *settings) {
    (void)settings;
    return 0;
}

#endif /* NATIVE_BUILD */

#endif /* NXCLIENT_H */