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
#ifndef NEXUS_SIMPLE_VIDEO_DECODER_H
#define NEXUS_SIMPLE_VIDEO_DECODER_H

#ifdef NATIVE_BUILD
/*
 * Stub header for Broadcom Nexus simple video decoder
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/* Constants */
#define NEXUS_SIMPLE_DECODER_MAX_SURFACES 6
#define NEXUS_OUT_OF_DEVICE_MEMORY (-100)
#define NEXUS_OFFSCREEN_SECURE_GRAPHICS_SURFACE 1
#define NEXUS_ANY_ID 0
#define NEXUS_NORMAL_DECODE_RATE 1000
#define NEXUS_GRAPHICS2D_QUEUED 1
#define BKNI_INFINITE 0xFFFFFFFF

/* Broadcom SDK utility macros */
#define BSTD_UNUSED(x) ((void)(x))
#define BKNI_SetEvent(e) ((void)(e))
#define BKNI_CreateEvent(e) (*(e) = (void*)1, 0)
#define BKNI_DestroyEvent(e) ((void)(e))
#define BKNI_WaitForEvent(e, t) ((void)(e), (void)(t), 0)

/* Error codes */
typedef int NEXUS_Error;
#define NEXUS_SUCCESS 0

/* Forward declarations */
typedef void* NEXUS_VideoDecoderHandle;
typedef void* NEXUS_SimpleVideoDecoderHandle;
typedef void* NEXUS_SimpleStcChannelHandle;
typedef void* NEXUS_PidChannelHandle;
typedef void* NEXUS_SurfaceHandle;
typedef void* NEXUS_SurfaceClientHandle;
typedef void* NEXUS_Graphics2DHandle;
typedef void* BKNI_EventHandle;
typedef void* NEXUS_HeapHandle;

/* Video Codec Enumeration */
typedef enum NEXUS_VideoCodec {
    NEXUS_VideoCodec_eUnknown = 0,
    NEXUS_VideoCodec_eMpeg1,
    NEXUS_VideoCodec_eMpeg2,
    NEXUS_VideoCodec_eMpeg4Part2,
    NEXUS_VideoCodec_eH263,
    NEXUS_VideoCodec_eH264,
    NEXUS_VideoCodec_eH264_Svc,
    NEXUS_VideoCodec_eH264_Mvc,
    NEXUS_VideoCodec_eH265,
    NEXUS_VideoCodec_eVc1,
    NEXUS_VideoCodec_eVc1SimpleMain,
    NEXUS_VideoCodec_eVp6,
    NEXUS_VideoCodec_eVp8,
    NEXUS_VideoCodec_eVp9,
    NEXUS_VideoCodec_eAvs,
    NEXUS_VideoCodec_eMotionJpeg,
    NEXUS_VideoCodec_eRv40,
    NEXUS_VideoCodec_eAv1,
    NEXUS_VideoCodec_eMax
} NEXUS_VideoCodec;

/* Video Format */
typedef enum NEXUS_VideoFormat {
    NEXUS_VideoFormat_eUnknown = 0,
    NEXUS_VideoFormat_eNtsc,
    NEXUS_VideoFormat_e1080i,
    NEXUS_VideoFormat_e720p,
    NEXUS_VideoFormat_e480p,
    NEXUS_VideoFormat_e1080p,
    NEXUS_VideoFormat_e3840x2160p24hz,
    NEXUS_VideoFormat_e3840x2160p30hz,
    NEXUS_VideoFormat_e3840x2160p60hz,
    NEXUS_VideoFormat_eMax
} NEXUS_VideoFormat;

/* Video EOTF (Electro-Optical Transfer Function) */
typedef enum NEXUS_VideoEotf {
    NEXUS_VideoEotf_eInvalid = 0,
    NEXUS_VideoEotf_eSdr,
    NEXUS_VideoEotf_eHdr,
    NEXUS_VideoEotf_eHdr10,
    NEXUS_VideoEotf_eHlg,
    NEXUS_VideoEotf_eSmpteSt2084,
    NEXUS_VideoEotf_eAribStdB67,
    NEXUS_VideoEotf_eMax
} NEXUS_VideoEotf;

/* Secure Video */
typedef enum NEXUS_SecureVideo {
    NEXUS_SecureVideo_eInsecure = 0,
    NEXUS_SecureVideo_eSecure,
    NEXUS_SecureVideo_eMax
} NEXUS_SecureVideo;

/* Dynamic Range Metadata Type */
typedef enum NEXUS_VideoDecoderDynamicRangeMetadataType {
    NEXUS_VideoDecoderDynamicRangeMetadataType_eNone = 0,
    NEXUS_VideoDecoderDynamicRangeMetadataType_eDolbyVision,
    NEXUS_VideoDecoderDynamicRangeMetadataType_eTechnicolorPrime,
    NEXUS_VideoDecoderDynamicRangeMetadataType_eMax
} NEXUS_VideoDecoderDynamicRangeMetadataType;

/* Video Decoder Channel Change Mode */
typedef enum NEXUS_VideoDecoder_ChannelChangeMode {
    NEXUS_VideoDecoder_ChannelChangeMode_eMute = 0,
    NEXUS_VideoDecoder_ChannelChangeMode_eMuteUntilFirstPicture,
    NEXUS_VideoDecoder_ChannelChangeMode_eHoldUntilTsmLock,
    NEXUS_VideoDecoder_ChannelChangeMode_eMax
} NEXUS_VideoDecoder_ChannelChangeMode;

/* Video Decoder Progressive Override Mode */
typedef enum NEXUS_VideoDecoderProgressiveOverrideMode {
    NEXUS_VideoDecoderProgressiveOverrideMode_eDisable = 0,
    NEXUS_VideoDecoderProgressiveOverrideMode_eTopFieldOnly,
    NEXUS_VideoDecoderProgressiveOverrideMode_eBottomFieldOnly,
    NEXUS_VideoDecoderProgressiveOverrideMode_eMax
} NEXUS_VideoDecoderProgressiveOverrideMode;

/* Video Decoder Timestamp Mode */
typedef enum NEXUS_VideoDecoderTimestampMode {
    NEXUS_VideoDecoderTimestampMode_eDecode = 0,
    NEXUS_VideoDecoderTimestampMode_eDisplay,
    NEXUS_VideoDecoderTimestampMode_eMax
} NEXUS_VideoDecoderTimestampMode;

/* Video Dynamic Range Mode */
typedef enum NEXUS_VideoDynamicRangeMode {
    NEXUS_VideoDynamicRangeMode_eSdr = 0,
    NEXUS_VideoDynamicRangeMode_eHdr10,
    NEXUS_VideoDynamicRangeMode_eHlg,
    NEXUS_VideoDynamicRangeMode_eDolbyVisionSourceLed,
    NEXUS_VideoDynamicRangeMode_eTrackInput,
    NEXUS_VideoDynamicRangeMode_eMax
} NEXUS_VideoDynamicRangeMode;

/* Pixel Format */
typedef enum NEXUS_PixelFormat {
    NEXUS_PixelFormat_eUnknown = 0,
    NEXUS_PixelFormat_eA8_R8_G8_B8,
    NEXUS_PixelFormat_eY08_Cr8_Y18_Cb8,
    NEXUS_PixelFormat_eMax
} NEXUS_PixelFormat;

/* TSM Mode */
typedef enum NEXUS_TsmMode {
    NEXUS_TsmMode_eDisabled = 0,
    NEXUS_TsmMode_eEnabled,
    NEXUS_TsmMode_eMax
} NEXUS_TsmMode;

/* Video Decoder Decode Mode */
typedef enum NEXUS_VideoDecoderDecodeMode {
    NEXUS_VideoDecoderDecodeMode_eAll = 0,
    NEXUS_VideoDecoderDecodeMode_eI,
    NEXUS_VideoDecoderDecodeMode_eIP,
    NEXUS_VideoDecoderDecodeMode_eMax
} NEXUS_VideoDecoderDecodeMode;

/* Video Decoder Scan Mode */
typedef enum NEXUS_VideoDecoderScanMode {
    NEXUS_VideoDecoderScanMode_e1080p = 0,
    NEXUS_VideoDecoderScanMode_e1080i,
    NEXUS_VideoDecoderScanMode_eMax
} NEXUS_VideoDecoderScanMode;

/* Video Decoder Low Latency Mode */
typedef enum NEXUS_VideoDecoderLowLatencyMode {
    NEXUS_VideoDecoderLowLatencyMode_eOff = 0,
    NEXUS_VideoDecoderLowLatencyMode_eAverage,
    NEXUS_VideoDecoderLowLatencyMode_eMax
} NEXUS_VideoDecoderLowLatencyMode;

/* Video Window Capture Mode */
typedef enum NEXUS_VideoWindowCaptureMode {
    NEXUS_VideoWindowCaptureMode_eOff = 0,
    NEXUS_VideoWindowCaptureMode_eOn,
    NEXUS_VideoWindowCaptureMode_eAuto,
    NEXUS_VideoWindowCaptureMode_eMax
} NEXUS_VideoWindowCaptureMode;

/* Audio Output Mode */
typedef enum NxClient_AudioOutputMode {
    NxClient_AudioOutputMode_ePcm = 0,
    NxClient_AudioOutputMode_ePassthrough,
    NxClient_AudioOutputMode_eMax
} NxClient_AudioOutputMode;

/* Callback Descriptor */
typedef struct NEXUS_CallbackDesc {
    void (*callback)(void *context, int param);
    void *context;
    int param;
} NEXUS_CallbackDesc;

/* Content Light Level */
typedef struct NEXUS_ContentLightLevel {
    uint16_t max;
    uint16_t maxFrameAverage;
} NEXUS_ContentLightLevel;

/* Coordinate structure for color primaries */
typedef struct NEXUS_ColorPrimary {
    uint16_t x;
    uint16_t y;
} NEXUS_ColorPrimary;

/* Luminance structure */
typedef struct NEXUS_Luminance {
    uint32_t max;
    uint32_t min;
} NEXUS_Luminance;

/* Mastering Display Color Volume - using nested structures */
typedef struct NEXUS_MasteringDisplayColorVolume {
    NEXUS_ColorPrimary redPrimary;
    NEXUS_ColorPrimary greenPrimary;
    NEXUS_ColorPrimary bluePrimary;
    NEXUS_ColorPrimary whitePoint;
    NEXUS_Luminance luminance;
} NEXUS_MasteringDisplayColorVolume;

/* Video Decoder Status */
typedef struct NEXUS_VideoDecoderStatus {
    uint32_t pts;
    uint32_t numDecoded;
    uint32_t numDisplayed;
    uint32_t queueDepth;
    uint32_t fifoDepth;
    uint32_t numDecodeErrors;
    uint32_t numDecodeDrops;
    uint32_t numDisplayDrops;
    uint32_t numDisplayErrors;
    uint32_t numPicturesReceived;
    uint64_t numBytesDecoded;
    uint32_t numDisplayUnderflows;
    bool started;
    bool firstPtsPassed;
    NEXUS_Rect source;
} NEXUS_VideoDecoderStatus;

/* Video Decoder Memory */
typedef struct NEXUS_VideoDecoderMemory {
    NEXUS_SecureVideo secure;
    uint32_t maxFormat;
} NEXUS_VideoDecoderMemory;

/* Video Decoder Capabilities */
typedef struct NEXUS_VideoDecoderCapabilities {
    NEXUS_VideoDecoderMemory memory[2];
    uint32_t colorDepth;
    uint32_t maxWidth;
    uint32_t maxHeight;
} NEXUS_VideoDecoderCapabilities;

/* Simple Video Decoder Client Status */
typedef struct NEXUS_SimpleVideoDecoderClientStatus {
    bool enabled;
    bool decoderResourcesAcquired;
} NEXUS_SimpleVideoDecoderClientStatus;

/* Video Decoder Stream Information */
typedef struct NEXUS_VideoDecoderStreamInformation {
    bool valid;
    uint32_t sourceHorizontalSize;
    uint32_t sourceVerticalSize;
    uint32_t codedSourceHorizontalSize;
    uint32_t codedSourceVerticalSize;
    uint32_t displayHorizontalSize;
    uint32_t displayVerticalSize;
    uint32_t aspectRatio;
    uint32_t sampleAspectRatioX;
    uint32_t sampleAspectRatioY;
    uint32_t frameRate;
    bool frameProgressive;
    bool streamProgressive;
    int horizontalPanScan;
    int verticalPanScan;
    bool lowDelayFlag;
    bool fixedFrameRateFlag;
    NEXUS_VideoEotf eotf;
    NEXUS_VideoDecoderDynamicRangeMetadataType dynamicMetadataType;
    uint32_t colorDepth;
} NEXUS_VideoDecoderStreamInformation;

/* Video Decoder Settings */
typedef struct NEXUS_VideoDecoderSettings {
    int ptsOffset;
    NEXUS_VideoDecoder_ChannelChangeMode channelChangeMode;
    NEXUS_VideoCodec codec;
    NEXUS_PidChannelHandle pidChannel;
    NEXUS_VideoDecoderProgressiveOverrideMode progressiveOverrideMode;
    NEXUS_VideoDecoderTimestampMode timestampMode;
    int prerollRate;
    NEXUS_VideoEotf eotf;
    NEXUS_MasteringDisplayColorVolume masteringDisplayColorVolume;
    NEXUS_ContentLightLevel contentLightLevel;
    NEXUS_VideoDecoderScanMode scanMode;
    NEXUS_CallbackDesc fifoEmpty;
    NEXUS_CallbackDesc firstPts;
    NEXUS_CallbackDesc ptsError;
    NEXUS_CallbackDesc streamChanged;
} NEXUS_VideoDecoderSettings;

/* Simple Video Decoder Start Settings */
typedef struct NEXUS_SimpleVideoDecoderStartSettings {
    NEXUS_VideoDecoderHandle videoDecoder;
    NEXUS_VideoDecoderSettings settings;
    bool displayEnabled;
    bool smoothResolutionChange;
    uint32_t maxWidth;
    uint32_t maxHeight;
} NEXUS_SimpleVideoDecoderStartSettings;

/* Simple Video Decoder Client Settings */
typedef struct NEXUS_SimpleVideoDecoderClientSettings {
    bool closedCaptionRouting;
    bool mtgAllowed;
    NEXUS_VideoWindowCaptureMode captureMode;
    NEXUS_CallbackDesc resourceChanged;
} NEXUS_SimpleVideoDecoderClientSettings;

/* Video Decoder Low Latency Settings */
typedef struct NEXUS_VideoDecoderLowLatencySettings {
    NEXUS_VideoDecoderLowLatencyMode mode;
    uint32_t latency;
} NEXUS_VideoDecoderLowLatencySettings;

/* Video Decoder Extended Settings */
typedef struct NEXUS_VideoDecoderExtendedSettings {
    NEXUS_CallbackDesc dataReadyCallback;
    bool zeroDelayOutputMode;
    NEXUS_VideoDecoderLowLatencySettings lowLatencySettings;
    bool ignoreDpbOutputDelaySyntax;
    bool earlyPictureDeliveryMode;
    bool treatIFrameAsRap;
    bool ignoreNumReorderFramesEqZero;
} NEXUS_VideoDecoderExtendedSettings;

/* Video Decoder Trick State */
typedef struct NEXUS_VideoDecoderTrickState {
    int rate;
    bool topFieldOnly;
    NEXUS_VideoDecoderDecodeMode decodeMode;
    bool stcTrickEnabled;
    NEXUS_TsmMode tsmEnabled;
} NEXUS_VideoDecoderTrickState;

/* Surface Create Settings */
typedef struct NEXUS_SurfaceCreateSettings {
    uint32_t width;
    uint32_t height;
    NEXUS_PixelFormat pixelFormat;
    NEXUS_HeapHandle heap;
} NEXUS_SurfaceCreateSettings;

/* Surface Memory */
typedef struct NEXUS_SurfaceMemory {
    void *buffer;
    uint32_t pitch;
    uint32_t numBytes;
} NEXUS_SurfaceMemory;

/* Simple Video Decoder Capture Status */
typedef struct NEXUS_SimpleVideoDecoderCaptureStatus {
    uint32_t pts;
    bool ptsValid;
    uint32_t serialNumber;
} NEXUS_SimpleVideoDecoderCaptureStatus;

/* Simple Video Decoder Start Capture Settings */
typedef struct NEXUS_SimpleVideoDecoderStartCaptureSettings {
    bool displayEnabled;
    bool secure;
    NEXUS_SurfaceHandle surface[NEXUS_SIMPLE_DECODER_MAX_SURFACES];
} NEXUS_SimpleVideoDecoderStartCaptureSettings;

/* StcChannel Settings */
typedef struct NEXUS_StcChannelMode {
    struct {
        int offsetThreshold;
    } Auto;
} NEXUS_StcChannelMode;

typedef struct NEXUS_SimpleStcChannelSettings {
    NEXUS_StcChannelMode modeSettings;
} NEXUS_SimpleStcChannelSettings;

/* Graphics2D Settings */

typedef struct NEXUS_Graphics2DOpenSettings {
    bool secure;
} NEXUS_Graphics2DOpenSettings;

typedef struct NEXUS_Graphics2DSettings {
    NEXUS_CallbackDesc checkpointCallback;
} NEXUS_Graphics2DSettings;

typedef struct NEXUS_Graphics2DBlitSource {
    NEXUS_SurfaceHandle surface;
    NEXUS_Rect rect;
} NEXUS_Graphics2DBlitSource;

typedef struct NEXUS_Graphics2DBlitOutput {
    NEXUS_SurfaceHandle surface;
    NEXUS_Rect rect;
} NEXUS_Graphics2DBlitOutput;

typedef struct NEXUS_Graphics2DBlitSettings {
    NEXUS_Graphics2DBlitSource source;
    NEXUS_Graphics2DBlitOutput output;
} NEXUS_Graphics2DBlitSettings;

/* NxClient Audio Settings */
typedef struct NxClient_HdmiAudioSettings {
    NxClient_AudioOutputMode outputMode;
} NxClient_HdmiAudioSettings;

typedef struct NxClient_AudioSettings {
    NxClient_HdmiAudioSettings hdmi;
} NxClient_AudioSettings;

/* Function Declarations */

/* Simple Video Decoder functions */
static inline void NEXUS_SimpleVideoDecoder_GetDefaultStartSettings(NEXUS_SimpleVideoDecoderStartSettings *settings) { 
    if(settings) { 
        memset(settings, 0, sizeof(NEXUS_SimpleVideoDecoderStartSettings));
        settings->displayEnabled = true;
    }
}

static inline NEXUS_SimpleVideoDecoderHandle NEXUS_SimpleVideoDecoder_Acquire(unsigned index) { 
    (void)index; 
    return (NEXUS_SimpleVideoDecoderHandle)1; 
}

static inline void NEXUS_SimpleVideoDecoder_Release(NEXUS_SimpleVideoDecoderHandle handle) { 
    (void)handle; 
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetStatus(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_VideoDecoderStatus *status) { 
    (void)handle; 
    if(status) { 
        memset(status, 0, sizeof(NEXUS_VideoDecoderStatus));
    } 
    return NEXUS_SUCCESS; 
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetClientStatus(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_SimpleVideoDecoderClientStatus *status) { 
    (void)handle; 
    if(status) { 
        status->enabled = true; 
        status->decoderResourcesAcquired = true; 
    } 
    return NEXUS_SUCCESS; 
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetStreamInformation(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_VideoDecoderStreamInformation *info) { 
    (void)handle; 
    if(info) { 
        memset(info, 0, sizeof(NEXUS_VideoDecoderStreamInformation));
        info->eotf = NEXUS_VideoEotf_eSdr; 
        info->dynamicMetadataType = NEXUS_VideoDecoderDynamicRangeMetadataType_eNone; 
    } 
    return NEXUS_SUCCESS; 
}

static inline NEXUS_Error NEXUS_GetVideoDecoderCapabilities(NEXUS_VideoDecoderCapabilities *cap) { 
    if(cap) { 
        cap->memory[0].secure = NEXUS_SecureVideo_eInsecure; 
        cap->memory[0].maxFormat = NEXUS_VideoFormat_e3840x2160p60hz;
        cap->memory[1].secure = NEXUS_SecureVideo_eInsecure; 
        cap->memory[1].maxFormat = NEXUS_VideoFormat_e1080p;
        cap->colorDepth = 8; 
        cap->maxWidth = 1920; 
        cap->maxHeight = 1080; 
    } 
    return NEXUS_SUCCESS; 
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetSettings(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_VideoDecoderSettings *settings) {
    (void)handle;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_VideoDecoderSettings));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetSettings(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_VideoDecoderSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetClientSettings(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_SimpleVideoDecoderClientSettings *settings) {
    (void)handle;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SimpleVideoDecoderClientSettings));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetExtendedSettings(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_VideoDecoderExtendedSettings *settings) {
    (void)handle;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_VideoDecoderExtendedSettings));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetExtendedSettings(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_VideoDecoderExtendedSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetClientSettings(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_SimpleVideoDecoderClientSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_Start(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_SimpleVideoDecoderStartSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_Stop(NEXUS_SimpleVideoDecoderHandle handle) {
    (void)handle;
    return NEXUS_SUCCESS;
}
static inline void NEXUS_StartCallbacks(NEXUS_SimpleVideoDecoderHandle handle) {
    (void)handle;
}

static inline void NEXUS_StopCallbacks(NEXUS_SimpleVideoDecoderHandle handle) {
    (void)handle;
}
static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetStcChannel(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_SimpleStcChannelHandle stc) {
    (void)handle;
    (void)stc;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_Flush(NEXUS_SimpleVideoDecoderHandle handle) {
    (void)handle;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetStartPts(NEXUS_SimpleVideoDecoderHandle handle, uint32_t pts) {
    (void)handle;
    (void)pts;
    return NEXUS_SUCCESS;
}

static inline void NEXUS_SimpleVideoDecoder_GetDefaultStartCaptureSettings(NEXUS_SimpleVideoDecoderStartCaptureSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SimpleVideoDecoderStartCaptureSettings));
    }
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_StartCapture(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_SimpleVideoDecoderStartCaptureSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_StopCapture(NEXUS_SimpleVideoDecoderHandle handle) {
    (void)handle;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetCapturedSurfaces(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_SurfaceHandle *surface, NEXUS_SimpleVideoDecoderCaptureStatus *status, uint32_t numEntries, uint32_t *numReturned) {
    (void)handle;
    (void)surface;
    (void)status;
    (void)numEntries;
    if(numReturned) *numReturned = 0;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_RecycleCapturedSurfaces(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_SurfaceHandle *surface, uint32_t numEntries) {
    (void)handle;
    (void)surface;
    (void)numEntries;
    return NEXUS_SUCCESS;
}

/* STC Channel functions */
static inline NEXUS_Error NEXUS_SimpleStcChannel_Freeze(NEXUS_SimpleStcChannelHandle stc, bool freeze) {
    (void)stc;
    (void)freeze;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleStcChannel_Invalidate(NEXUS_SimpleStcChannelHandle stc) {
    (void)stc;
    return NEXUS_SUCCESS;
}

/* Surface functions */
static inline void NEXUS_Surface_GetDefaultCreateSettings(NEXUS_SurfaceCreateSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SurfaceCreateSettings));
    }
}

static inline NEXUS_SurfaceHandle NEXUS_Surface_Create(const NEXUS_SurfaceCreateSettings *settings) {
    (void)settings;
    return (NEXUS_SurfaceHandle)1;
}

static inline void NEXUS_Surface_Destroy(NEXUS_SurfaceHandle surface) {
    (void)surface;
}

static inline NEXUS_Error NEXUS_Surface_Lock(NEXUS_SurfaceHandle surface, NEXUS_SurfaceMemory *mem) {
    (void)surface;
    if(mem) {
        memset(mem, 0, sizeof(NEXUS_SurfaceMemory));
    }
    return NEXUS_SUCCESS;
}

static inline void NEXUS_Surface_Unlock(NEXUS_SurfaceHandle surface) {
    (void)surface;
}

/* Platform functions */
static inline NEXUS_HeapHandle NEXUS_Platform_GetFramebufferHeap(uint32_t index) {
    (void)index;
    return (NEXUS_HeapHandle)1;
}

/* NxClient Surface Client Composition functions */
static inline NEXUS_Error NxClient_GetSurfaceClientComposition(uint32_t surfaceClientId, NEXUS_SurfaceComposition *composition) {
    (void)surfaceClientId;
    if(composition) {
        memset(composition, 0, sizeof(NEXUS_SurfaceComposition));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NxClient_SetSurfaceClientComposition(uint32_t surfaceClientId, const NEXUS_SurfaceComposition *composition) {
    (void)surfaceClientId;
    (void)composition;
    return NEXUS_SUCCESS;
}

static inline void NxClient_GetAudioSettings(NxClient_AudioSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NxClient_AudioSettings));
    }
}

/* SimpleStcChannel functions */
static inline NEXUS_Error NEXUS_SimpleStcChannel_GetStc(NEXUS_SimpleStcChannelHandle stc, uint32_t *value) {
    (void)stc;
    if(value) *value = 0;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleStcChannel_GetSettings(NEXUS_SimpleStcChannelHandle stc, NEXUS_SimpleStcChannelSettings *settings) {
    (void)stc;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_SimpleStcChannelSettings));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleStcChannel_SetStc(NEXUS_SimpleStcChannelHandle stc, uint32_t value) {
    (void)stc;
    (void)value;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleStcChannel_SetRate(NEXUS_SimpleStcChannelHandle stc, uint32_t numerator, uint32_t denominator) {
    (void)stc;
    (void)numerator;
    (void)denominator;
    return NEXUS_SUCCESS;
}

/* SimpleVideoDecoder additional functions */
static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetTrickState(NEXUS_SimpleVideoDecoderHandle handle, NEXUS_VideoDecoderTrickState *state) {
    (void)handle;
    if(state) {
        memset(state, 0, sizeof(NEXUS_VideoDecoderTrickState));
        state->rate = NEXUS_NORMAL_DECODE_RATE;
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_SetTrickState(NEXUS_SimpleVideoDecoderHandle handle, const NEXUS_VideoDecoderTrickState *state) {
    (void)handle;
    (void)state;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_SimpleVideoDecoder_GetNextPts(NEXUS_SimpleVideoDecoderHandle handle, uint32_t *pts) {
    (void)handle;
    if(pts) *pts = 0;
    return NEXUS_SUCCESS;
}

/* Surface additional functions */
static inline NEXUS_Error NEXUS_Surface_GetMemory(NEXUS_SurfaceHandle surface, NEXUS_SurfaceMemory *mem) {
    (void)surface;
    if(mem) {
        memset(mem, 0, sizeof(NEXUS_SurfaceMemory));
    }
    return NEXUS_SUCCESS;
}

static inline void NEXUS_Surface_Flush(NEXUS_SurfaceHandle surface) {
    (void)surface;
}

/* Graphics2D functions */
static inline void NEXUS_Graphics2D_GetDefaultOpenSettings(NEXUS_Graphics2DOpenSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_Graphics2DOpenSettings));
    }
}

static inline NEXUS_Graphics2DHandle NEXUS_Graphics2D_Open(uint32_t index, const NEXUS_Graphics2DOpenSettings *settings) {
    (void)index;
    (void)settings;
    return (NEXUS_Graphics2DHandle)1;
}

static inline void NEXUS_Graphics2D_Close(NEXUS_Graphics2DHandle handle) {
    (void)handle;
}

static inline NEXUS_Error NEXUS_Graphics2D_GetSettings(NEXUS_Graphics2DHandle handle, NEXUS_Graphics2DSettings *settings) {
    (void)handle;
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_Graphics2DSettings));
    }
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_Graphics2D_SetSettings(NEXUS_Graphics2DHandle handle, const NEXUS_Graphics2DSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline void NEXUS_Graphics2D_GetDefaultBlitSettings(NEXUS_Graphics2DBlitSettings *settings) {
    if(settings) {
        memset(settings, 0, sizeof(NEXUS_Graphics2DBlitSettings));
    }
}

static inline NEXUS_Error NEXUS_Graphics2D_Blit(NEXUS_Graphics2DHandle handle, const NEXUS_Graphics2DBlitSettings *settings) {
    (void)handle;
    (void)settings;
    return NEXUS_SUCCESS;
}

static inline NEXUS_Error NEXUS_Graphics2D_Checkpoint(NEXUS_Graphics2DHandle handle, void *unused) {
    (void)handle;
    (void)unused;
    return NEXUS_SUCCESS;
}

#endif /* NATIVE_BUILD */

#endif /* NEXUS_SIMPLE_VIDEO_DECODER_H */