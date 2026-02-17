#ifndef OMX_BROADCOM_H
#define OMX_BROADCOM_H

#ifdef NATIVE_BUILD

#include "IL/OMX_Core.h"

/*
 * Stub header for Broadcom-specific OMX extensions
 * Required by Raspberry Pi variant
 */

/* Broadcom-specific OMX component names */
#define OMX_BROADCOM_VIDEO_DECODE_COMPONENT_NAME "OMX.broadcom.video_decode"
#define OMX_BROADCOM_VIDEO_RENDER_COMPONENT_NAME "OMX.broadcom.video_render"
#define OMX_BROADCOM_VIDEO_SCHEDULER_COMPONENT_NAME "OMX.broadcom.video_scheduler"
#define OMX_BROADCOM_AUDIO_RENDER_COMPONENT_NAME "OMX.broadcom.audio_render"

/* Broadcom-specific event types */
typedef enum {
    OMX_EventParamOrConfigChanged = 0x7F000001
} OMX_BROADCOM_EVENTTYPE;

/* Broadcom-specific error types */
typedef enum {
    OMX_ErrorBrcmHardware = 0x80001000
} OMX_BROADCOM_ERRORTYPE;

/* Broadcom-specific port types */
typedef enum {
    OMX_BROADCOM_PortParam_VideoDecodeOutputPort = 0x7F000001,
    OMX_BROADCOM_PortParam_VideoRenderInputPort,
    OMX_BROADCOM_PortParam_VideoSchedulerInputPort,
    OMX_BROADCOM_PortParam_VideoSchedulerOutputPort,
    OMX_BROADCOM_PortParam_ImageDecoderInputPort,
    OMX_BROADCOM_PortParam_ImageDecoderOutputPort
} OMX_BROADCOM_PORTPARAM;

/* Broadcom parameter structures */
typedef struct OMX_BROADCOM_CONFIG_DISPLAYREGION {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL fullscreen;
    OMX_U32 dest_rect_x;
    OMX_U32 dest_rect_y;
    OMX_U32 dest_rect_width;
    OMX_U32 dest_rect_height;
    OMX_U32 src_rect_x;
    OMX_U32 src_rect_y;
    OMX_U32 src_rect_width;
    OMX_U32 src_rect_height;
    OMX_U32 noaspect;
    OMX_U32 transform;
    OMX_U32 layer;
    OMX_S32 opacity;
} OMX_BROADCOM_CONFIG_DISPLAYREGION;

typedef struct OMX_BROADCOM_CONFIG_COLORSPACE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nColorspace;
} OMX_BROADCOM_CONFIG_COLORSPACE;

typedef struct OMX_BROADCOM_PARAM_THUMBNAIL {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnabled;
} OMX_BROADCOM_PARAM_THUMBNAIL;

/* Broadcom specific indices (OMX_IndexConfigDisplayRegion is in OMX_Core.h) */
typedef enum {
    OMX_IndexParamVideoDecodeErrorConcealment = 0x7F010001,
    OMX_IndexConfigBufferStall = 0x7F010002,
    OMX_IndexParamBrcmRenderStats = 0x7F000002,
    OMX_IndexConfigBrcmAudioDownmixCoefficients = 0x7F000003,
    OMX_IndexParamBrcmVideoSource = 0x7F000004,
    OMX_IndexConfigBrcmColorspace = 0x7F000005,
    OMX_IndexParamBrcmThumbnail = 0x7F000006
} OMX_BROADCOM_INDEXTYPE;

/* Broadcom-specific callback extensions */
typedef struct {
    OMX_ERRORTYPE (*EventHandler)(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                 OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData);
    OMX_ERRORTYPE (*EmptyBufferDone)(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer);
    OMX_ERRORTYPE (*FillBufferDone)(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer);
    OMX_ERRORTYPE (*ConfigChanged)(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_U32 nIndex, OMX_PTR pComponentConfigStructure);
} OMX_BROADCOM_CALLBACKTYPE;

#endif /* NATIVE_BUILD */

#endif /* OMX_BROADCOM_H */