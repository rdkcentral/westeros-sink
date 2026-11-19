/*
 * Vendor-private link between westerossink and the capture library.
 *
 * MW hands westerossink an opaque ICaptureSession pointer through the
 * rdk-capture-session property. westerossink is C and cannot call the C++
 * session, and the specification keeps scheduler/backing access private to
 * the vendor (section 4.2), so the sink reaches the session through this table.
 *
 * The sink dlopen()s the capture library (RDK_CAPTURE_LIBRARY, default
 * libcapture-emu.so; the soname matches the copy MW already loaded, so both
 * share one instance), resolves RDK_CAPTURE_SINK_BRIDGE_SYMBOL and calls it to
 * get the table. Not part of the MW-facing HAL.
 */
#ifndef RDK_CAPTURE_CAPTURE_SINK_BRIDGE_H
#define RDK_CAPTURE_CAPTURE_SINK_BRIDGE_H

#include <stdint.h>
#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RDK_CAPTURE_DEFAULT_LIBRARY "libcapture-emu.so"
#define RDK_CAPTURE_LIBRARY_ENV "RDK_CAPTURE_LIBRARY"
#define RDK_CAPTURE_SINK_BRIDGE_SYMBOL "rdkCaptureSinkBridgeV1"
#define RDK_CAPTURE_SINK_BRIDGE_VERSION 2

typedef struct RdkCaptureSinkLink RdkCaptureSinkLink;

typedef struct RdkCaptureSinkBridge
{
   uint32_t version;

   /* NULL->READY. Associates session with this sink's platform video output,
    * after which getCapabilities() and createPool() are valid (configured
    * READY, section 4.4).
    * Returns NULL if session is not a live session of this library, is
    * already connected, or was connected before (sessions are never
    * reassociated). */
   RdkCaptureSinkLink *(*connect)(void *session, GstElement *sink);

   /* READY->NULL. Stops publication; the final selection, frame locks and
    * late releases stay valid in the session. Frees link. */
   void (*disconnect)(RdkCaptureSinkLink *link);

   /* Extra decoder buffers the sink must request so that retained frames
    * never starve decode (section 7.5). Valid from connect(). */
   uint32_t (*retentionHeadroom)(RdkCaptureSinkLink *link);

   /* Negotiated caps. Returns TRUE if they are capturable (DMA_DRM, within
    * the declared envelope). Before the pool exists, non-capturable caps make
    * getCapabilities() return UNSUPPORTED; after it, they make the session
    * capture-fatal (section 7.1). A change of backing is reported by the next
    * frame's dmaBufObjectsFlushed; a colour-only change is not. */
   gboolean (*configure)(RdkCaptureSinkLink *link, GstCaps *caps);

   /* Every buffer reaching the sink, before synchronisation. Records the
    * plane layout; a layout differing from the current one is a new
    * configuration, reported by the next frame's dmaBufObjectsFlushed. The
    * pool and its capacity stay as created. */
   void (*observe)(RdkCaptureSinkLink *link, GstBuffer *buffer);

   /* The buffer platform video output selected for now, or the prerolled
    * buffer while paused. Becomes the current selection once the pool is
    * created; ignored before, after a capture-fatal transition, and when it
    * already is the selection. */
   void (*publish)(RdkCaptureSinkLink *link, GstBuffer *buffer);

   /* Flush/seek: the current selection is gone; acquisition returns
    * NO_FRAME until the next publish. Retained frames are unaffected. */
   void (*invalidateSelection)(RdkCaptureSinkLink *link);
} RdkCaptureSinkBridge;

typedef const RdkCaptureSinkBridge *(*RdkCaptureSinkBridgeGetter)(void);

#ifdef __cplusplus
}
#endif

#endif /* RDK_CAPTURE_CAPTURE_SINK_BRIDGE_H */
