/*
 * Broadcom-private extension of the capture sink bridge.
 *
 * The generic RdkCaptureSinkBridge (rdk-capture/src/capture-sink-bridge.h) covers
 * connect/disconnect/flush.  The Broadcom backend additionally has to take over the
 * NEXUS_SimpleVideoDecoder (buffer mode, display disabled), which the generic table
 * does not describe.  westeros-sink-soc.c resolves this table with dlsym() from the
 * same library handle it got the generic bridge from.
 */
#ifndef CAPTURE_BROADCOM_EXT_H
#define CAPTURE_BROADCOM_EXT_H

#include <stdint.h>
#include "capture-sink-bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BRCM_CAPTURE_DEFAULT_LIBRARY "libcapture-broadcom.so"
#define BRCM_CAPTURE_EXT_SYMBOL "rdkCaptureBroadcomExtV1"
#define BRCM_CAPTURE_EXT_VERSION 2

typedef struct BrcmCaptureExt
{
   uint32_t version;

   /* NEXUS_SimpleStcChannelHandle the decoder is synchronised to (NULL clears it).
    * Call before prepareBufferCapture(). */
   void (*setStcChannel)(RdkCaptureSinkLink *link, void *stcChannel);

   /* Decoder configured with bufferMode=true and displayEnabled=false. prepare is
    * called before NEXUS_SimpleVideoDecoder_Start(), start after it succeeded.
    * "simpleVideoDecoder" is the NEXUS_SimpleVideoDecoderHandle. 1 = ok, 0 = failure. */
   int (*prepareBufferCapture)(RdkCaptureSinkLink *link, void *simpleVideoDecoder);
   int (*startBufferCapture)(RdkCaptureSinkLink *link, void *simpleVideoDecoder);

   /* Must be called before NEXUS_SimpleVideoDecoder_Stop(). Safe when not running. */
   void (*stopBufferCapture)(RdkCaptureSinkLink *link);
} BrcmCaptureExt;

typedef const BrcmCaptureExt *(*BrcmCaptureExtGetter)(void);

#ifdef __cplusplus
}
#endif

#endif
