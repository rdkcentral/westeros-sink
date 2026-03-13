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
/* Stub header for Raspberry Pi OMX Core */
#ifndef OMX_CORE_H
#define OMX_CORE_H

#ifdef NATIVE_BUILD

#include <stdint.h>
#include <string.h>

struct wl_display;
struct wl_display *khrn_platform_get_wl_display(void);

/* OMX parameter qualifiers */
#define OMX_IN
#define OMX_OUT
#define OMX_INOUT

/* Basic OMX type definitions */
typedef void* OMX_HANDLETYPE;
typedef char* OMX_STRING;
typedef void* OMX_PTR;
typedef uint32_t OMX_U32;
typedef uint16_t OMX_U16;
typedef uint8_t OMX_U8;
typedef int32_t OMX_S32;
typedef int16_t OMX_S16;
typedef int8_t OMX_S8;
typedef uint64_t OMX_U64;
typedef int OMX_BOOL;
typedef uint8_t OMX_UUIDTYPE[128];

/* OMX Error types */
typedef enum OMX_ERRORTYPE {
    OMX_ErrorNone = 0,
    OMX_ErrorInsufficientResources = 1,
    OMX_ErrorUndefined = 2,
    OMX_ErrorComponentNotFound = 3,
    OMX_ErrorInvalidComponentName = 4,
    OMX_ErrorComponentNotRole = 5,
    OMX_ErrorInvalidState = 6,
    OMX_ErrorHardware = 7,
    OMX_ErrorStreamCorrupt = 8,
    OMX_ErrorBadParameter = 9,
    OMX_ErrorNotImplemented = 10,
    OMX_ErrorVersionMismatch = 11,
    OMX_ErrorNotReady = 12,
    OMX_ErrorTimeout = 13
} OMX_ERRORTYPE;

/* OMX Event types */
typedef enum OMX_EVENTTYPE {
    OMX_EventCmdComplete = 0,
    OMX_EventError = 1,
    OMX_EventMark = 2,
    OMX_EventPortSettingsChanged = 3,
    OMX_EventPortFormatDetected = 4,
    OMX_EventResourcesAcquired = 5,
    OMX_EventComponentResumed = 6,
    OMX_EventDynamicResourcesAvailable = 7,
    OMX_EventOutputDropped = 8,
    OMX_EventMax = 0x7FFFFFFF
} OMX_EVENTTYPE;

/* OMX State types */
typedef enum OMX_STATETYPE {
    OMX_StateInvalid = 0,
    OMX_StateLoaded = 1,
    OMX_StateIdle = 2,
    OMX_StateExecuting = 3,
    OMX_StatePause = 4,
    OMX_StateWaitForResources = 5,
    OMX_StateMax = 0x7FFFFFFF
} OMX_STATETYPE;

/* OMX Command types */
typedef enum OMX_COMMANDTYPE {
    OMX_CommandStateSet = 0,
    OMX_CommandFlush = 1,
    OMX_CommandPortDisable = 2,
    OMX_CommandPortEnable = 3,
    OMX_CommandMarkBuffer = 4,
    OMX_CommandMax = 0x7FFFFFFF
} OMX_COMMANDTYPE;

/* OMX version structure */
typedef struct {
    union {
        struct {
            OMX_U32 nVersionMajor;
            OMX_U32 nVersionMinor;
            OMX_U32 nRevision;
            OMX_U32 nStep;
        } s;
        OMX_U32 nVersion;
    };
} OMX_VERSIONTYPE;

/* Forward declaration for buffer header type - needed before OMX_COMPONENTTYPE */
typedef struct OMX_BUFFERHEADERTYPE OMX_BUFFERHEADERTYPE;
typedef void OMX_TUNNELEDCOMPONENTTYPE;

/* Forward declaration for callback type - needed before OMX_COMPONENTTYPE */
typedef struct OMX_CALLBACKTYPE OMX_CALLBACKTYPE;

/* Full definition of buffer header type */
typedef struct OMX_BUFFERHEADERTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U8 *pBuffer;
    OMX_U32 nAllocLen;
    OMX_U32 nFilledLen;
    OMX_U32 nOffset;
    OMX_PTR pAppPrivate;
    OMX_PTR pPlatformPrivate;
    OMX_PTR pInputPortPrivate;
    OMX_PTR pOutputPortPrivate;
    OMX_U32 nInputPortIndex;
    OMX_U32 nOutputPortIndex;
    OMX_U32 nFlags;
    OMX_U32 nTickCount;
    union {
        OMX_U64 nValue;
        struct {
            OMX_U32 nLowPart;
            OMX_U32 nHighPart;
        };
    } nTimeStamp;
    void *hMarkTargetComponent;
    OMX_U32 nMarkData;
} OMX_BUFFERHEADERTYPE;

/* OMX Callback type */
typedef struct OMX_CALLBACKTYPE {
    OMX_ERRORTYPE (*EventHandler)(OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE, OMX_U32, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*EmptyBufferDone)(OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE*);
    OMX_ERRORTYPE (*FillBufferDone)(OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE*);
} OMX_CALLBACKTYPE;

/* OMX component type */
typedef struct OMX_COMPONENTTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_PTR pApplicationPrivate;
    OMX_PTR pComponentPrivate;
    OMX_ERRORTYPE (*GetComponentVersion)(OMX_HANDLETYPE, OMX_STRING, OMX_VERSIONTYPE*, OMX_VERSIONTYPE*, OMX_VERSIONTYPE*);
    OMX_ERRORTYPE (*SendCommand)(OMX_HANDLETYPE, OMX_COMMANDTYPE, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*GetParameter)(OMX_HANDLETYPE, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*SetParameter)(OMX_HANDLETYPE, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*GetConfig)(OMX_HANDLETYPE, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*SetConfig)(OMX_HANDLETYPE, OMX_U32, OMX_PTR);
    OMX_ERRORTYPE (*GetExtensionIndex)(OMX_HANDLETYPE, OMX_STRING, OMX_U32*);
    OMX_ERRORTYPE (*GetState)(OMX_HANDLETYPE, OMX_STATETYPE*);
    OMX_ERRORTYPE (*ComponentTunnelRequest)(OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32, OMX_TUNNELEDCOMPONENTTYPE*);
    OMX_ERRORTYPE (*UseBuffer)(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32, OMX_U8*);
    OMX_ERRORTYPE (*AllocateBuffer)(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, OMX_U32);
    OMX_ERRORTYPE (*FreeBuffer)(OMX_HANDLETYPE, OMX_U32, OMX_BUFFERHEADERTYPE*);
    OMX_ERRORTYPE (*EmptyThisBuffer)(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE*);
    OMX_ERRORTYPE (*FillThisBuffer)(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE*);
    OMX_ERRORTYPE (*SetCallbacks)(OMX_HANDLETYPE, OMX_CALLBACKTYPE*, OMX_PTR);
    OMX_ERRORTYPE (*ComponentDeInit)(OMX_HANDLETYPE);
    OMX_ERRORTYPE (*UseEGLImage)(OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE**, OMX_U32, OMX_PTR, void*);
} OMX_COMPONENTTYPE;

/* OMX Port parameter type */
typedef struct OMX_PORT_PARAM_TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPorts;
    OMX_U32 nStartPortNumber;
} OMX_PORT_PARAM_TYPE;

/* OMX Port definition type */
typedef struct OMX_PARAM_PORTDEFINITIONTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 eDir;
    OMX_U32 nBufferCountActual;
    OMX_U32 nBufferCountMin;
    OMX_U32 nBufferSize;
    OMX_U32 nBufferAlignment;
    OMX_U32 bEnabled;
    OMX_U32 bPopulated;
    OMX_U32 eDomain;
    union {
        struct {
            OMX_U32 nFrameWidth;
            OMX_U32 nFrameHeight;
            OMX_U32 xFramerate;
            OMX_U32 eColorFormat;
            OMX_U32 eCompressionFormat;
        } video;
        struct {
            OMX_U32 nChannels;
            OMX_U32 nSampleRate;
            OMX_U32 nBitPerSample;
            OMX_U32 bInterleaved;
            OMX_U32 eChannelMapping;
        } audio;
    } format;
} OMX_PARAM_PORTDEFINITIONTYPE;

/* OMX Video compression formats */
typedef enum OMX_VIDEO_CODINGTYPE {
    OMX_VIDEO_CodingUnused = 0,
    OMX_VIDEO_CodingAutoDetect = 1,
    OMX_VIDEO_CodingMPEG2 = 2,
    OMX_VIDEO_CodingH263 = 3,
    OMX_VIDEO_CodingMPEG4 = 4,
    OMX_VIDEO_CodingWMV = 5,
    OMX_VIDEO_CodingRV = 6,
    OMX_VIDEO_CodingAVC = 7,
    OMX_VIDEO_CodingMJPEG = 8,
    OMX_VIDEO_CodingVP8 = 9,
    OMX_VIDEO_CodingVP9 = 10,
    OMX_VIDEO_CodingHEVC = 11,
    OMX_VIDEO_CodingKhronosExtensions = 0x6F000000,
    OMX_VIDEO_CodingVendorStartUnused = 0x7F000000,
    OMX_VIDEO_CodingMax = 0x7FFFFFFF
} OMX_VIDEO_CODINGTYPE;

/* OMX Video port format type */
typedef struct OMX_VIDEO_PARAM_PORTFORMATTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nIndex;
    OMX_VIDEO_CODINGTYPE eCompressionFormat;
    OMX_U32 eColorFormat;
    OMX_U32 xFramerate;
} OMX_VIDEO_PARAM_PORTFORMATTYPE;

/* OMX Index types - Parameter indices */
typedef enum OMX_INDEXTYPE {
    OMX_IndexParamPriorityMgmt = 0,
    OMX_IndexParamAudioInit = 1,
    OMX_IndexParamImageInit = 2,
    OMX_IndexParamVideoInit = 3,
    OMX_IndexParamOtherInit = 4,
    OMX_IndexParamPortDefinition = 5,
    OMX_IndexParamVideoPortFormat = 6,
    OMX_IndexParamVideoQuantization = 7,
    OMX_IndexParamContentPipe = 8,
    OMX_IndexConfigTimeClockState = 0x7F000001,
    OMX_IndexConfigTimeScale = 0x7F000002,
    OMX_IndexConfigTimeCurrentMediaTime = 0x7F000003,
    OMX_IndexConfigDisplayRegion = 0x7F000004,
    OMX_IndexMax = 0x7FFFFFFF
} OMX_INDEXTYPE;

/* OMX Port direction */
typedef enum OMX_DIRTYPE {
    OMX_DirInput = 0,
    OMX_DirOutput = 1,
    OMX_DirMax = 0x7FFFFFFF
} OMX_DIRTYPE;

/* OMX Time clock states */
typedef enum OMX_TIME_CLOCKSTATE {
    OMX_TIME_ClockStateRunning = 0,
    OMX_TIME_ClockStateWaitingForStartTime = 1,
    OMX_TIME_ClockStateStopped = 2,
    OMX_TIME_ClockStateMax = 0x7FFFFFFF
} OMX_TIME_CLOCKSTATE;

/* OMX Clock port */
#define OMX_CLOCKPORT0 0

/* OMX Version macro - packed as single U32 */
#define OMX_VERSION 0x01000000

/* OMX Timestamp type for media time - defined early for use in clock config */
typedef union {
    struct {
        OMX_U32 nLowPart;
        OMX_U32 nHighPart;
    };  /* Anonymous struct allows direct field access */
    OMX_U64 nValue;
} OMX_TIME_TIMESTAMPTYPE;

/* OMX Time clock config */
typedef struct OMX_TIME_CONFIG_CLOCKSTATETYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_TIME_CLOCKSTATE eState;
    OMX_U32 nWaitMask;
    OMX_TIME_TIMESTAMPTYPE nOffset;
} OMX_TIME_CONFIG_CLOCKSTATETYPE;

/* OMX Time scale config */
typedef struct OMX_TIME_CONFIG_SCALETYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 xScale;
} OMX_TIME_CONFIG_SCALETYPE;

/* OMX Time config for current media timestamp */
typedef struct OMX_TIME_CONFIG_TIMESTAMPTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_TIME_TIMESTAMPTYPE nTimestamp;
} OMX_TIME_CONFIG_TIMESTAMPTYPE;

/* OMX Additional events */
#define OMX_EventBufferFlag 0x7F000001

/* OMX Boolean constants */
#define OMX_TRUE  1
#define OMX_FALSE 0

/* OMX Buffer flags */
#define OMX_BUFFERFLAG_EOS 0x00000001
#define OMX_BUFFERFLAG_STARTTIME 0x00000002
#define OMX_BUFFERFLAG_DECODEONLY 0x00000004
#define OMX_BUFFERFLAG_DATACORRUPT 0x00000008
#define OMX_BUFFERFLAG_ENDOFFRAME 0x00000010
#define OMX_BUFFERFLAG_SYNCFRAME 0x00000020

/* OMX Display set types */
typedef enum {
    OMX_DISPLAY_SET_NONE = 0,
    OMX_DISPLAY_SET_FULLSCREEN = 0x00000001,
    OMX_DISPLAY_SET_TRANSFORM = 0x00000002,
    OMX_DISPLAY_SET_DEST_RECT = 0x00000004,
    OMX_DISPLAY_SET_SRC_RECT = 0x00000008,
    OMX_DISPLAY_SET_NOASPECT = 0x00000010,
    OMX_DISPLAY_SET_MODE = 0x00000020,
    OMX_DISPLAY_SET_PIXEL = 0x00000040,
    OMX_DISPLAY_SET_ROTATION = 0x00000080,
    OMX_DISPLAY_SET_LAYER = 0x00000100,
    OMX_DISPLAY_SET_ALPHA = 0x00000200,
    OMX_DISPLAY_SET_OPACITY = 0x00000400
} OMX_DISPLAYSETTYPE;

/* Display region rectangle */
typedef struct {
    OMX_S32 x_offset;
    OMX_S32 y_offset;
    OMX_U32 width;
    OMX_U32 height;
} OMX_DISPLAYREGION_RECT;

/* Display region config type */
typedef struct OMX_CONFIG_DISPLAYREGIONTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_DISPLAYSETTYPE set;
    OMX_BOOL fullscreen;
    OMX_U32 transform;
    OMX_DISPLAYREGION_RECT dest_rect;
    OMX_DISPLAYREGION_RECT src_rect;
    OMX_BOOL noaspect;
    OMX_U32 mode;
    OMX_U32 pixel_x;
    OMX_U32 pixel_y;
    OMX_U32 layer;
    OMX_S32 opacity;
    OMX_U32 rotation;
} OMX_CONFIG_DISPLAYREGIONTYPE;

/* Basic OMX API stubs */
static inline OMX_ERRORTYPE OMX_Init(void) { 
    return OMX_ErrorNone; 
}

static inline OMX_ERRORTYPE OMX_Deinit(void) { 
    return OMX_ErrorNone; 
}

static inline OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE *pHandle, OMX_STRING cComponentName, 
                                         OMX_PTR pAppData, OMX_CALLBACKTYPE *pCallBacks) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE hComponent) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_SendCommand(OMX_HANDLETYPE hComponent, OMX_COMMANDTYPE Cmd,
                                           OMX_U32 nParam, OMX_PTR pCmdData) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_GetState(OMX_HANDLETYPE hComponent, OMX_STATETYPE *pState) {
    if (pState) *pState = OMX_StateExecuting;
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_GetComponentVersion(OMX_HANDLETYPE hComponent, OMX_STRING cComponentName,
                                                   OMX_VERSIONTYPE *pComponentVersion,
                                                   OMX_VERSIONTYPE *pSpecVersion,
                                                   OMX_UUIDTYPE *pImplementationVersion) {
    if (pComponentVersion) {
        pComponentVersion->s.nVersionMajor = 1;
        pComponentVersion->s.nVersionMinor = 0;
        pComponentVersion->s.nRevision = 0;
        pComponentVersion->s.nStep = 0;
    }
    if (pSpecVersion) {
        pSpecVersion->s.nVersionMajor = 1;
        pSpecVersion->s.nVersionMinor = 0;
        pSpecVersion->s.nRevision = 0;
        pSpecVersion->s.nStep = 0;
    }
    if (pImplementationVersion) {
        /* UID is not set in stub, just provide placeholder */
        memset(pImplementationVersion, 0, sizeof(OMX_UUIDTYPE));
    }
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_GetParameter(OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex, 
                                            OMX_PTR pComponentParameterStructure) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_SetParameter(OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,
                                            OMX_PTR pComponentParameterStructure) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_GetConfig(OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,
                                         OMX_PTR pComponentConfigStructure) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_SetConfig(OMX_HANDLETYPE hComponent, OMX_INDEXTYPE nParamIndex,
                                         OMX_PTR pComponentConfigStructure) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_AllocateBuffer(OMX_HANDLETYPE hComponent, 
                                              OMX_BUFFERHEADERTYPE **pBuffer,
                                              OMX_U32 nPortIndex, OMX_PTR pAppPrivate,
                                              OMX_U32 nSizeBytes) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_FreeBuffer(OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex,
                                          OMX_BUFFERHEADERTYPE *pBuffer) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_EmptyThisBuffer(OMX_HANDLETYPE hComponent,
                                               OMX_BUFFERHEADERTYPE *pBuffer) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_FillThisBuffer(OMX_HANDLETYPE hComponent,
                                              OMX_BUFFERHEADERTYPE *pBuffer) {
    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE OMX_SetupTunnel(OMX_HANDLETYPE hOutput, OMX_U32 nOutputPortIndex,
                                           OMX_HANDLETYPE hInput, OMX_U32 nInputPortIndex) {
    return OMX_ErrorNone;
}

#endif /* NATIVE_BUILD */

#endif /* OMX_CORE_H */
