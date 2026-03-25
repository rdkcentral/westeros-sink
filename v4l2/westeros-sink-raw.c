/*
 * Copyright (C) 2020 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <drm/drm_fourcc.h>
#include <xf86drm.h>

#ifdef USE_GST_VIDEO
#include <gst/video/gstvideometa.h>
#endif

#ifdef USE_GST_AFD
#include "gst/video/video-anc.h"
#ifndef gst_buffer_get_video_afd_meta
#undef USE_GST_AFD
#endif
#endif

#ifdef USE_GST_ALLOCATORS
#include <gst/allocators/gstdmabuf.h>
#endif

#include "westeros-sink.h"

#define DEFAULT_VIDEO_SERVER "video"
#define DEFAULT_OVERSCAN (0)

#ifdef GLIB_VERSION_2_32
  #define LOCK_RAW( sink ) g_mutex_lock( &((sink)->raw.mutex) );
  #define UNLOCK_RAW( sink ) g_mutex_unlock( &((sink)->raw.mutex) );
#else
  #define LOCK_RAW( sink ) g_mutex_lock( (sink)->raw.mutex );
  #define UNLOCK_RAW( sink ) g_mutex_unlock( (sink)->raw.mutex );
#endif

GST_DEBUG_CATEGORY_EXTERN (gst_westeros_sink_debug);
#define GST_CAT_DEFAULT gst_westeros_sink_debug

#define INT_FRAME(FORMAT, ...)      frameLog( "FRAME: " FORMAT "\n", __VA_ARGS__)
#define FRAME(...)                  INT_FRAME(__VA_ARGS__, "")

enum
{
  PROP_DEVICE= PROP_RAW_BASE,
  #ifdef USE_AMLOGIC_MESON_MSYNC
  PROP_AVSYNC_SESSION,
  PROP_AVSYNC_MODE,
  #endif
  PROP_FORCE_ASPECT_RATIO,
  PROP_WINDOW_SHOW,
  PROP_ZOOM_MODE,
  PROP_OVERSCAN_SIZE,
  PROP_ENABLE_TEXTURE,
  PROP_STATS
};
enum
{
   SIGNAL_FIRSTFRAME,
   SIGNAL_UNDERFLOW,
   SIGNAL_NEWTEXTURE,
   SIGNAL_TIMECODE,
   MAX_SIGNAL
};
enum
{
   ZOOM_NONE,
   ZOOM_DIRECT,
   ZOOM_NORMAL,
   ZOOM_16_9_STRETCH,
   ZOOM_4_3_PILLARBOX,
   ZOOM_ZOOM,
   ZOOM_GLOBAL
};

#define needBounds(sink) ( sink->raw.forceAspectRatio || (sink->raw.zoomMode != ZOOM_NONE) )

static bool g_frameDebug= false;
static guint g_signals[MAX_SIGNAL]= {0};

static void wstSinkRawStopVideo( GstWesterosSink *sink );
static void wstGetVideoBounds( GstWesterosSink *sink, int *x, int *y, int *w, int *h );
static void wstSetTextureCrop( GstWesterosSink *sink, int vx, int vy, int vw, int vh );
static WstRawVideoClientConnection *wstCreateVideoClientConnection( GstWesterosSink *sink, const char *name );
static void wstDestroyVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstSendResourceVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstSendHideVideoClientConnection( WstRawVideoClientConnection *conn, bool hide );
static void wstSendSessionInfoVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstSetSessionInfo( GstWesterosSink *sink );
static void wstSendFlushVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstSendPauseVideoClientConnection( WstRawVideoClientConnection *conn, bool pause );
static void wstSendRectVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstSendRateVideoClientConnection( WstRawVideoClientConnection *conn );
static void wstProcessMessagesVideoClientConnection( WstRawVideoClientConnection *conn );
static bool wstSendFrameVideoClientConnection( WstRawVideoClientConnection *conn, int buffIndex );
static gpointer wstDispatchThread(gpointer data);
static gpointer wstEOSDetectionThread(gpointer data);
static gpointer wstFirstFrameThread(gpointer data);
static gpointer wstUnderflowThread(gpointer data);
// static void wstBuildSinkCaps_raw( GstWesterosSinkClass *klass );
static bool drmInit( GstWesterosSink *sink );
static void drmTerm( GstWesterosSink *sink );
static bool drmAllocBuffer( GstWesterosSink *sink, int buffIndex, int width, int height );
static void drmFreeBuffer( GstWesterosSink *sink, int buffIndex );
static void drmLockBuffer( GstWesterosSink *sink, int buffIndex );
static bool drmUnlockBuffer( GstWesterosSink *sink, int buffIndex );
static void drmUnlockAllBuffers( GstWesterosSink *sink );
#ifdef USE_GST_ALLOCATORS
static WstDrmBuffer *drmImportBuffer( GstWesterosSink *sink, GstBuffer *buffer );
#endif
static WstDrmBuffer *drmGetBuffer( GstWesterosSink *sink, int width, int height );
static void drmReleaseBuffer( GstWesterosSink *sink, int buffIndex );
static int sinkAcquireResources( GstWesterosSink *sink );
static void sinkReleaseResources( GstWesterosSink *sink );
static int sinkAcquireVideo( GstWesterosSink *sink );
static void sinkReleaseVideo( GstWesterosSink *sink );
static GstStructure *wstSinkGetStats( GstWesterosSink * sink );
#ifdef USE_GENERIC_AVSYNC
static void wstPruneAVSyncFiles( GstWesterosSink *sink );
static AVSyncCtx* wstCreateAVSyncCtx( GstWesterosSink *sink );
static void wstDestroyAVSyncCtx( GstWesterosSink *sink, AVSyncCtx *avsctx );
static void wstUpdateAVSyncCtx( GstWesterosSink *sink, AVSyncCtx *avsctx );
#endif

#ifdef USE_AMLOGIC_MESON
#ifdef USE_AMLOGIC_MESON_MSYNC
#define INVALID_SESSION_ID (16)
#include "gstamlclock.h"
#include "gstamlhalasink_new.h"
#endif
#endif

static long long getCurrentTimeMillis(void)
{
   struct timeval tv;
   long long utcCurrentTimeMillis;

   gettimeofday(&tv,0);
   utcCurrentTimeMillis= tv.tv_sec*1000LL+(tv.tv_usec/1000LL);

   return utcCurrentTimeMillis;
}

static gint64 getGstClockTime( GstWesterosSink *sink )
{
   gint64 time= 0;
   GstElement *element= GST_ELEMENT(sink);
   GstClock *clock= GST_ELEMENT_CLOCK(element);
   if ( clock )
   {
      time= gst_clock_get_time(clock);
   }
   return time;
}

static void frameLog( const char *fmt, ... )
{
   if ( g_frameDebug )
   {
      va_list argptr;
      fprintf( stderr, "%lld: ", getCurrentTimeMillis());
      va_start( argptr, fmt );
      vfprintf( stderr, fmt, argptr );
      va_end( argptr );
   }
}

static void sbFormat(void *data, struct wl_sb *wl_sb, uint32_t format)
{
   WESTEROS_UNUSED(wl_sb);
   GstWesterosSink *sink= (GstWesterosSink*)data;
   WESTEROS_UNUSED(sink);
   printf("westeros-sink-raw: registry: sbFormat: %X\n", format);
}

static const struct wl_sb_listener sbListener = {
	sbFormat
};

typedef struct bufferInfo
{
   GstWesterosSink *sink;
   int buffIndex;
} bufferInfo;

static void buffer_release( void *data, struct wl_buffer *buffer )
{
   int rc;
   bufferInfo *binfo= (bufferInfo*)data;

   GstWesterosSink *sink= binfo->sink;

   if (binfo->buffIndex >= 0)
   {
      FRAME("out:       wayland release received for buffer %d", binfo->buffIndex);
      LOCK(sink);
      if ( drmUnlockBuffer( sink, binfo->buffIndex ) )
      {
         drmReleaseBuffer( sink, binfo->buffIndex );
      }
      UNLOCK(sink);
   }

   wl_buffer_destroy( buffer );

   free( binfo );
}

static struct wl_buffer_listener wl_buffer_listener=
{
   buffer_release
};

static bool wstApproxEqual( double v1, double v2 )
{
   bool result= false;
   if ( v1 >= v2 )
   {
      if ( (v1-v2) < 0.01 )
      {
         result= true;
      }
   }
   else
   {
      if ( (v2-v1) < 0.01 )
      {
         result= true;
      }
   }
   return result;
}

#ifdef USE_GST_AFD
static void wstSetAFDInfo( GstWesterosSink *sink, GstBuffer *buffer )
{
   GstVideoAFDMeta* afd;
   GstVideoBarMeta* bar;
   bool haveNew= false;

   afd= gst_buffer_get_video_afd_meta(buffer);
   bar= gst_buffer_get_video_bar_meta(buffer);

   if ( afd || bar )
   {
      WstAFDInfo *afdCurr= &sink->raw.afdActive;

      if ( afd )
      {
         if ( afd->afd != afdCurr->afd )
         {
            haveNew= true;
         }
      }
      if ( bar )
      {
         if ( !afdCurr->haveBar )
         {
            haveNew= true;
         }
         else if ( (afdCurr->isLetterbox != bar->is_letterbox) ||
                   (afdCurr->d1 != bar->bar_data1) ||
                   (afdCurr->d2 != bar->bar_data2) )
         {
            haveNew= true;
         }
      }

      if ( haveNew )
      {
         gint64 pts= -1;
         if ( GST_BUFFER_PTS_IS_VALID(buffer) )
         {
            pts= GST_BUFFER_PTS(buffer);
         }
         memset( afdCurr, 0, sizeof(WstAFDInfo));
         afdCurr->pts= pts;
         afdCurr->frameNumber= sink->raw.frameInCount;
         if ( afd )
         {
            afdCurr->spec= afd->spec;
            afdCurr->afd= afd->afd;
            afdCurr->field= afd->field;
         }
         if ( bar )
         {
            afdCurr->haveBar= true;
            afdCurr->isLetterbox= bar->is_letterbox;
            afdCurr->d1= bar->bar_data1;
            afdCurr->d2= bar->bar_data2;
            afdCurr->f= bar->field;
         }
         GST_DEBUG("active AFD pts %lld frame %d afd %d field %d/%d", afdCurr->pts, afdCurr->frameNumber, afdCurr->afd, afdCurr->field, afdCurr->f);
      }
   }
}

static void wstFlushAFDInfo( GstWesterosSink *sink )
{
   GST_DEBUG("flush AFD info");
   memset( &sink->raw.afdActive, 0, sizeof(WstAFDInfo));
}
#endif

void gst_westeros_sink_raw_class_init(GstWesterosSinkClass *klass)
{
   GObjectClass *gobject_class= (GObjectClass *) klass;
   GstBaseSinkClass *gstbasesink_class= (GstBaseSinkClass *) klass;
   GstElementClass *Element_class= GST_ELEMENT_CLASS(klass);
   guint sid= 0;

   // gst_element_class_set_static_metadata( GST_ELEMENT_CLASS(klass),
   //                                        "Westeros Sink",
   //                                        "Sink/Video",
   //                                        "Writes buffers to the westeros wayland compositor",
   //                                        "Comcast" );

   // Get the pad template from soc class and use it for raw class as well.
   
   // if ( !gst_element_class_get_pad_template(Element_class, "sink") )
   // {
   //    wstBuildSinkCaps_raw( klass );
   // }
#if 0
   #ifdef USE_AMLOGIC_MESON_MSYNC
      if ( !g_object_class_find_property(gobject_class, "avsync-session") )
      {
         g_object_class_install_property (gobject_class, PROP_AVSYNC_SESSION,
         g_param_spec_int ("avsync-session", "avsync session",
                           "avsync session id to link video and audio. If set, this sink won't look for it from audio sink",
                           G_MININT, G_MAXINT, 0, G_PARAM_WRITABLE));
      }
      if ( !g_object_class_find_property(gobject_class, "avsync-mode") )
      {
         g_object_class_install_property (gobject_class, PROP_AVSYNC_MODE,
         g_param_spec_int ("avsync-mode", "avsync mode",
                           "Vmaster(0) Amaster(1) PCRmaster(2) IPTV(3) FreeRun(4)",
                           G_MININT, G_MAXINT, 0, G_PARAM_WRITABLE));
      }
   #endif
   
   if ( !g_object_class_find_property(gobject_class, "enable-texture") )
   {
      g_object_class_install_property (gobject_class, PROP_ENABLE_TEXTURE,
      g_param_spec_boolean ("enable-texture",
                              "enable texture signal",
                              "0: disable; 1: enable", FALSE, G_PARAM_READWRITE));
   }

   if ( !g_object_class_find_property(gobject_class, "force-aspect-ratio") )
   {
      g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
                            "force aspect ratio",
                            "When enabled scaling respects source aspect ratio", FALSE, G_PARAM_READWRITE));
   }

   if ( !g_object_class_find_property(gobject_class, "show-video-window") )
   {
      g_object_class_install_property (gobject_class, PROP_WINDOW_SHOW,
        g_param_spec_boolean ("show-video-window",
                              "make video window visible",
                              "true: visible, false: hidden", TRUE, G_PARAM_WRITABLE));
   }

   if ( !g_object_class_find_property(gobject_class, "zoom-mode") )
   {
      g_object_class_install_property (gobject_class, PROP_ZOOM_MODE,
      g_param_spec_int ("zoom-mode",
                        "zoom-mode",
                        "Set zoom mode: 0-none, 1-direct, 2-normal, 3-16x9 stretch, 4-4x3 pillar box, 5-zoom, 6-global",
                        ZOOM_NONE, ZOOM_GLOBAL, ZOOM_NONE, G_PARAM_READWRITE));
   }

   if ( !g_object_class_find_property(gobject_class, "overscan-size") )
   {
      g_object_class_install_property (gobject_class, PROP_OVERSCAN_SIZE,
     g_param_spec_int ("overscan-size",
                       "overscan-size",
                       "Set overscan size to be used with applicable zoom-modes",
                       0, 10, DEFAULT_OVERSCAN, G_PARAM_READWRITE));
   }

#if GST_CHECK_VERSION(1, 18, 0)
   if ( !g_object_class_find_property(gobject_class, "stats") )
   {
      g_object_class_override_property (gobject_class, PROP_STATS, "stats");
   }
#else
   if ( !g_object_class_find_property(gobject_class, "stats") )
   {
      g_object_class_install_property (gobject_class, PROP_STATS,
        g_param_spec_boxed ("stats", "Statistics",
          "Sink Statistics", GST_TYPE_STRUCTURE,
           (GParamFlags)(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
   }
#endif

// Commenting all the callbacks not as to repeat it again in raw sink.
   // Reuse existing signal if already registered by SOC
   sid= g_signal_lookup("first-video-frame-callback", G_TYPE_FROM_CLASS(Element_class));
   g_signals[SIGNAL_FIRSTFRAME]= sid ? sid :
      g_signal_new( "first-video-frame-callback",
                  G_TYPE_FROM_CLASS(GST_ELEMENT_CLASS(klass)),
                  (GSignalFlags) (G_SIGNAL_RUN_LAST),
                  0,    /* class offset */
                  NULL, /* accumulator */
                  NULL, /* accu data */
                  g_cclosure_marshal_VOID__UINT_POINTER,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_UINT,
                  G_TYPE_POINTER );

   sid= g_signal_lookup("buffer-underflow-callback", G_TYPE_FROM_CLASS(Element_class));
   g_signals[SIGNAL_UNDERFLOW]= sid ? sid :
      g_signal_new( "buffer-underflow-callback",
                  G_TYPE_FROM_CLASS(GST_ELEMENT_CLASS(klass)),
                  (GSignalFlags) (G_SIGNAL_RUN_LAST),
                  0,    // class offset
                  NULL, // accumulator
                  NULL, // accu data
                  g_cclosure_marshal_VOID__UINT_POINTER,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_UINT,
                  G_TYPE_POINTER );

   sid= g_signal_lookup("new-video-texture-callback", G_TYPE_FROM_CLASS(Element_class));
   g_signals[SIGNAL_NEWTEXTURE]= sid ? sid :
      g_signal_new( "new-video-texture-callback",
                  G_TYPE_FROM_CLASS(GST_ELEMENT_CLASS(klass)),
                  (GSignalFlags) (G_SIGNAL_RUN_LAST),
                  0,    /* class offset */
                  NULL, /* accumulator */
                  NULL, /* accu data */
                  NULL,
                  G_TYPE_NONE,
                  15,
                  G_TYPE_UINT, /* format: fourcc */
                  G_TYPE_UINT, /* pixel width */
                  G_TYPE_UINT, /* pixel height */
                  G_TYPE_INT,  /* plane 0 fd */
                  G_TYPE_UINT, /* plane 0 byte length */
                  G_TYPE_UINT, /* plane 0 stride */
                  G_TYPE_POINTER, /* plane 0 data */
                  G_TYPE_INT,  /* plane 1 fd */
                  G_TYPE_UINT, /* plane 1 byte length */
                  G_TYPE_UINT, /* plane 1 stride */
                  G_TYPE_POINTER, /* plane 1 data */
                  G_TYPE_INT,  /* plane 2 fd */
                  G_TYPE_UINT, /* plane 2 byte length */
                  G_TYPE_UINT, /* plane 2 stride */
                  G_TYPE_POINTER /* plane 2 data */
               );

   #ifdef USE_GST_VIDEO
   sid= g_signal_lookup("timecode-callback", G_TYPE_FROM_CLASS(Element_class));
   g_signals[SIGNAL_TIMECODE]= sid ? sid :
      g_signal_new( "timecode-callback",
                  G_TYPE_FROM_CLASS(GST_ELEMENT_CLASS(klass)),
                  (GSignalFlags) (G_SIGNAL_RUN_LAST),
                  0,    /* class offset */
                  NULL, /* accumulator */
                  NULL, /* accu data */
                  NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_UINT, /* hours */
                  G_TYPE_UINT, /* minutes */
                  G_TYPE_UINT  /* seconds */
               );
   #endif

   klass->canUseResMgr= 0;
   {
      const char *env= getenv("WESTEROS_SINK_USE_ESSRMGR");
      if ( env && (atoi(env) != 0) )
      {
         klass->canUseResMgr= 1;
      }
   }
#endif
}

gboolean gst_westeros_sink_raw_init( GstWesterosSink *sink )
{
   GST_INFO("westeros-sink-raw.c: initializing raw backend");
   gboolean result= FALSE;
   const char *env;
   int rc;

   #ifdef GLIB_VERSION_2_32
   g_mutex_init( &sink->raw.mutex );
   #else
   sink->raw.mutex= g_mutex_new();
   #endif

   sink->raw.sb= 0;
   sink->raw.frameRate= 0.0;
   sink->raw.frameRateFractionNum= 0;
   sink->raw.frameRateFractionDenom= 0;
   sink->raw.frameRateChanged= FALSE;
   sink->raw.pixelAspectRatio= 1.0;
   sink->raw.havePixelAspectRatio= FALSE;
   sink->raw.pixelAspectRatioChanged= FALSE;
   #ifdef USE_GST_AFD
   memset( &sink->raw.afdActive, 0, sizeof(WstAFDInfo));
   #endif
   sink->raw.showChanged= FALSE;
   sink->raw.zoomModeGlobal= FALSE;
   sink->raw.zoomMode= ZOOM_NONE;
   sink->raw.zoomModeUser= -1;;
   sink->raw.overscanSize= DEFAULT_OVERSCAN;
   sink->raw.frameWidth= -1;
   sink->raw.frameHeight= -1;
   sink->raw.frameFormatStream= 0;
   sink->raw.frameFormatOut= 0;
   sink->raw.frameInCount= 0;
   sink->raw.frameOutCount= 0;
   sink->raw.frameDisplayCount= 0;
   sink->raw.numDropped= 0;
   sink->raw.currentInputPTS= 0;
   sink->raw.haveHardware= FALSE;
   sink->raw.useTunnelled= FALSE;
   sink->raw.expectDummyBuffers= FALSE;
   sink->raw.allow4kZoom= FALSE;

   sink->raw.updateSession= FALSE;
   sink->raw.syncType= -1;
   #ifdef USE_AMLOGIC_MESON_MSYNC
   sink->raw.sessionId= -1;
   #else
   sink->raw.sessionId= 0;
   #endif
   sink->raw.videoPlaying= FALSE;
   sink->raw.videoPaused= FALSE;
   sink->raw.quitEOSDetectionThread= FALSE;
   sink->raw.quitDispatchThread= FALSE;
   sink->raw.eosDetectionThread= NULL;
   sink->raw.dispatchThread= NULL;
   sink->raw.emitFirstFrameSignal= FALSE;
   sink->raw.emitUnderflowSignal= FALSE;
   sink->raw.nextFrameFd= -1;
   sink->raw.prevFrame1Fd= -1;
   sink->raw.prevFrame2Fd= -1;
   sink->raw.resubFd= -1;
   sink->raw.captureEnabled= FALSE;
   sink->raw.useCaptureOnly= FALSE;
   sink->raw.hideVideoFramesDelay= 2;
   sink->raw.hideGfxFramesDelay= 1;
   sink->raw.framesBeforeHideVideo= 0;
   sink->raw.framesBeforeHideGfx= 0;
   sink->raw.prevFrameTimeGfx= 0;
   sink->raw.prevFramePTSGfx= 0;
   sink->raw.videoX= sink->windowX;
   sink->raw.videoY= sink->windowY;
   sink->raw.videoWidth= sink->windowWidth;
   sink->raw.videoHeight= sink->windowHeight;
   sink->raw.forceAspectRatio= FALSE;
   sink->raw.drmFd= -1;
   sink->raw.nextDrmBuffer= 0;
   sink->raw.firstFrameThread= NULL;
   sink->raw.underflowThread= NULL;
   #ifdef USE_GENERIC_AVSYNC
   sink->raw.avsctx= 0;
   #endif
   {
      int i;
      for( i= 0; i < WST_NUM_DRM_BUFFERS; ++i )
      {
         sink->raw.drmBuffer[i].buffIndex= i;
         sink->raw.drmBuffer[i].locked= false;
         sink->raw.drmBuffer[i].lockCount= 0;
         sink->raw.drmBuffer[i].width= -1;
         sink->raw.drmBuffer[i].height= -1;
         sink->raw.drmBuffer[i].fd[0]= -1;
         sink->raw.drmBuffer[i].fd[1]= -1;
         sink->raw.drmBuffer[i].handle[0]= 0;
         sink->raw.drmBuffer[i].handle[1]= 0;
         sink->raw.drmBuffer[i].gstbuf= 0;
         sink->raw.drmBuffer[i].localAlloc= false;
      }
   }
   rc= sem_init( &sink->raw.drmBuffSem, 0, WST_NUM_DRM_BUFFERS );
   if ( !rc )
   {
      sink->raw.haveDrmBuffSem= true;
   }
   else
   {
      sink->raw.haveDrmBuffSem= false;
      GST_ERROR( "sem_init failed for drmBuffSem rc %d", rc);
   }

   sink->useSegmentPosition= TRUE;

   sink->acquireResources= sinkAcquireVideo;
   sink->releaseResources= sinkReleaseVideo;

   /* Request caps updates */
   sink->passCaps= TRUE;

   gst_base_sink_set_sync(GST_BASE_SINK(sink), TRUE);

   if ( getenv("WESTEROS_SINK_USE_FREERUN") )
   {
       gst_base_sink_set_sync(GST_BASE_SINK(sink), FALSE);
       printf("westeros-sink: using freerun\n");
   }
   else
   {
       gst_base_sink_set_sync(GST_BASE_SINK(sink), TRUE);
   }

   gst_base_sink_set_async_enabled(GST_BASE_SINK(sink), TRUE);

   if ( getenv("WESTEROS_SINK_USE_GFX") )
   {
      sink->raw.useCaptureOnly= TRUE;
      sink->raw.captureEnabled= TRUE;
      printf("westeros-sink: capture only\n");
   }

   #ifdef USE_AMLOGIC_MESON_MSYNC
   printf("westeros-sink: msync enabled\n");
   #endif

   env= getenv( "WESTEROS_SINK_DEBUG_FRAME" );
   if ( env )
   {
      int level= atoi( env );
      g_frameDebug= (level > 0 ? true : false);
   }

   #ifdef USE_GENERIC_AVSYNC
   wstPruneAVSyncFiles( sink );
   #endif

   result= TRUE;

   return result;
}

void gst_westeros_sink_raw_term( GstWesterosSink *sink )
{
   if ( sink->raw.haveDrmBuffSem )
   {
      sink->raw.haveDrmBuffSem= false;
      sem_destroy( &sink->raw.drmBuffSem );
   }
   #ifdef GLIB_VERSION_2_32
   g_mutex_clear( &sink->raw.mutex );
   #else
   g_mutex_free( sink->raw.mutex );
   #endif
}

void gst_westeros_sink_raw_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
   GstWesterosSink *sink = GST_WESTEROS_SINK(object);

   WESTEROS_UNUSED(pspec);

   switch (prop_id)
   {
      #ifdef USE_AMLOGIC_MESON_MSYNC
      case PROP_AVSYNC_SESSION:
         {
            int id= g_value_get_int(value);
            if (id >= 0)
            {
               sink->raw.userSession= TRUE;
               sink->raw.sessionId= id;
               GST_WARNING("AV sync session %d", id);
            }
            break;
         }
      case PROP_AVSYNC_MODE:
         {
            int mode= g_value_get_int(value);
            if (mode >= 0)
            {
               sink->raw.syncType= mode;
               GST_WARNING("AV sync mode %d", mode);
            }
            break;
         }
      #endif
      case PROP_FORCE_ASPECT_RATIO:
         {
            sink->raw.forceAspectRatio= g_value_get_boolean(value);
            break;
         }
      case PROP_WINDOW_SHOW:
         {
            gboolean show= g_value_get_boolean(value);
            if ( sink->show != show )
            {
               GST_DEBUG("set show-video-window to %d", show);
               sink->raw.showChanged= TRUE;
               sink->show= show;

               sink->visible= sink->show;
            }
         }
         break;
      case PROP_ZOOM_MODE:
         {
            gint zoom= g_value_get_int(value);
            sink->raw.zoomModeUser= zoom;
            if ( zoom == ZOOM_GLOBAL )
            {
               GST_DEBUG("enable global zoom");
               sink->raw.zoomModeGlobal= TRUE;
            }
            else
            {
               if ( sink->raw.zoomModeGlobal )
               {
                  GST_DEBUG("disable global zoom");
                  sink->raw.zoomModeGlobal= FALSE;
               }
               GST_DEBUG("set zoom-mode to %d", zoom);
               sink->raw.zoomMode= zoom;
            }
         }
         break;
      case PROP_OVERSCAN_SIZE:
         {
            gint overscan= g_value_get_int(value);
            if ( sink->raw.overscanSize != overscan )
            {
               GST_DEBUG("set overscan-size to %d", overscan);
               sink->raw.overscanSize= overscan;
            }
         }
         break;
      case PROP_ENABLE_TEXTURE:
         {
            sink->raw.enableTextureSignal= g_value_get_boolean(value);
         }
         break;
      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
         break;
   }
}

void gst_westeros_sink_raw_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
   GstWesterosSink *sink = GST_WESTEROS_SINK(object);

   WESTEROS_UNUSED(pspec);

   switch (prop_id)
   {
      #ifdef USE_AMLOGIC_MESON_MSYNC
      case PROP_AVSYNC_SESSION:
         g_value_set_int(value, sink->raw.sessionId);
         break;
      case PROP_AVSYNC_MODE:
         g_value_set_int(value, sink->raw.syncType);
         break;
      #endif
      case PROP_FORCE_ASPECT_RATIO:
         g_value_set_boolean(value, sink->raw.forceAspectRatio);
         break;
      case PROP_WINDOW_SHOW:
         g_value_set_boolean(value, sink->show);
         break;
      case PROP_ZOOM_MODE:
         g_value_set_int(value, sink->raw.zoomMode);
         break;
      case PROP_OVERSCAN_SIZE:
         g_value_set_int(value, sink->raw.overscanSize);
         break;
      case PROP_ENABLE_TEXTURE:
         g_value_set_boolean(value, sink->raw.enableTextureSignal);
         break;
      case PROP_STATS:
         {
            LOCK(sink);
            g_value_take_boxed( value, wstSinkGetStats(sink) );
            UNLOCK(sink);
         }
         break;
      default:
         G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
         break;
   }
}

void gst_westeros_sink_raw_registryHandleGlobal( GstWesterosSink *sink,
                                 struct wl_registry *registry, uint32_t id,
		                           const char *interface, uint32_t version)
{
   WESTEROS_UNUSED(version);
   int len;

   len= strlen(interface);

   if ((len==5) && (strncmp(interface, "wl_sb", len) == 0))
   {
      sink->raw.sb= (struct wl_sb*)wl_registry_bind(registry, id, &wl_sb_interface, version);
      printf("westeros-sink-raw: registry: sb %p\n", (void*)sink->raw.sb);
      wl_proxy_set_queue((struct wl_proxy*)sink->raw.sb, sink->queue);
		wl_sb_add_listener(sink->raw.sb, &sbListener, sink);
		printf("westeros-sink-raw: registry: done add sb listener\n");
   }

   if ( sink->raw.useCaptureOnly )
   {
      /* Don't use vpc when capture only */
      if ( sink->vpc )
      {
         wl_vpc_destroy( sink->vpc );
         sink->vpc= 0;
      }
   }
}

void gst_westeros_sink_raw_registryHandleGlobalRemove( GstWesterosSink *sink,
                                 struct wl_registry *registry,
			                        uint32_t name)
{
   WESTEROS_UNUSED(sink);
   WESTEROS_UNUSED(registry);
   WESTEROS_UNUSED(name);
}

gboolean gst_westeros_sink_raw_null_to_ready( GstWesterosSink *sink, gboolean *passToDefault )
{
   gboolean result= FALSE;

   WESTEROS_UNUSED(passToDefault);
   if ( sinkAcquireResources( sink ) )
   {
      result= TRUE;
   }
   else
   {
      GST_ERROR("gst_westeros_sink_null_to_ready: sinkAcquireResources failed");
   }

   if ( drmInit( sink ) )
   {
      result= TRUE;
   }

   return result;
}

gboolean gst_westeros_sink_raw_ready_to_paused( GstWesterosSink *sink, gboolean *passToDefault )
{
   WESTEROS_UNUSED(passToDefault);

   if ( !sink->raw.useCaptureOnly )
   {
      sink->raw.conn= wstCreateVideoClientConnection( sink, DEFAULT_VIDEO_SERVER );
      if ( !sink->raw.conn )
      {
         GST_ERROR("unable to connect to video server (%s)", DEFAULT_VIDEO_SERVER );
         sink->raw.useCaptureOnly= TRUE;
         sink->raw.captureEnabled= TRUE;
         printf("westeros-sink: no video server - capture only\n");
         if ( sink->vpc )
         {
            wl_vpc_destroy( sink->vpc );
            sink->vpc= 0;
         }
      }
   }

   LOCK(sink);
   sink->startAfterCaps= TRUE;
   sink->raw.videoPlaying= TRUE;
   sink->raw.videoPaused= FALSE;
   UNLOCK(sink);

   return TRUE;
}

gboolean gst_westeros_sink_raw_paused_to_playing( GstWesterosSink *sink, gboolean *passToDefault )
{
   WESTEROS_UNUSED(passToDefault);

   LOCK( sink );
   sink->raw.videoPlaying= TRUE;
   sink->raw.videoPaused= FALSE;
   #ifdef USE_AMLOGIC_MESON_MSYNC
   if ( !sink->raw.userSession )
   #endif
   {
      sink->raw.updateSession= TRUE;
   }
   UNLOCK( sink );
   wstSendPauseVideoClientConnection( sink->raw.conn, false );

   return TRUE;
}

gboolean gst_westeros_sink_raw_playing_to_paused( GstWesterosSink *sink, gboolean *passToDefault )
{
   LOCK( sink );
   sink->raw.videoPlaying= FALSE;
   sink->raw.videoPaused= TRUE;
   UNLOCK( sink );

   wstSendPauseVideoClientConnection( sink->raw.conn, true );

   if (gst_base_sink_is_async_enabled(GST_BASE_SINK(sink)))
   {
       /* To complete transition to paused state in async_enabled mode, we need a preroll buffer pushed to the pad;
          This is a workaround to avoid the need for preroll buffer. */
       GstBaseSink *basesink;
       basesink = GST_BASE_SINK(sink);
       GST_BASE_SINK_PREROLL_LOCK (basesink);
       basesink->have_preroll = 1;
       GST_BASE_SINK_PREROLL_UNLOCK (basesink);
      *passToDefault= true;
   }
   else
   {
      *passToDefault = false;
   }

   return TRUE;
}

gboolean gst_westeros_sink_raw_paused_to_ready( GstWesterosSink *sink, gboolean *passToDefault )
{
   wstSinkRawStopVideo( sink );
   LOCK( sink );
   sink->videoStarted= FALSE;
   UNLOCK( sink );

   if (gst_base_sink_is_async_enabled(GST_BASE_SINK(sink)))
   {
      *passToDefault= true;
   }
   else
   {
      *passToDefault= false;
   }

   return TRUE;
}

gboolean gst_westeros_sink_raw_ready_to_null( GstWesterosSink *sink, gboolean *passToDefault )
{
   WESTEROS_UNUSED(sink);

   wstSinkRawStopVideo( sink );

   drmTerm( sink );

   *passToDefault= false;

   return TRUE;
}

gboolean gst_westeros_sink_raw_accept_caps( GstWesterosSink *sink, GstCaps *caps )
{
   bool result= FALSE;
   GstStructure *structure;
   const gchar *mime;
   int len;

   gchar *str= gst_caps_to_string(caps);
   g_print("westeros-sink: caps: (%s)\n", str);
   g_free( str );

   structure= gst_caps_get_structure(caps, 0);
   if( structure )
   {
      mime= gst_structure_get_name(structure);
      if ( mime )
      {
         len= strlen(mime);
         if ( (len == 11) && !strncmp("video/x-raw", mime, len) )
         {
            result= TRUE;
         }
         else if ( (len == 20) && !strncmp("video/x-westeros-raw", mime, len) )
         {
            sink->raw.useTunnelled= TRUE;
            sink->raw.expectDummyBuffers= TRUE;
            sink->raw.frameFormatStream= DRM_FORMAT_NV12;
            result= TRUE;
         }
         else
         {
            GST_ERROR("gst_westeros_sink_raw_accept_caps: not accepting caps (%s)", mime );
         }
      }

      if ( result == TRUE )
      {
         gint num, denom, width, height;
         const gchar *format= 0;
         if ( gst_structure_get_fraction( structure, "framerate", &num, &denom ) )
         {
            if ( denom == 0 ) denom= 1;
            sink->raw.frameRate= (double)num/(double)denom;
            if ( sink->raw.frameRate <= 0.0 )
            {
               g_print("westeros-sink: caps have framerate of 0 - assume 60\n");
               sink->raw.frameRate= 60.0;
            }
            if ( (sink->raw.frameRateFractionNum != num) || (sink->raw.frameRateFractionDenom != denom) )
            {
               sink->raw.frameRateFractionNum= num;
               sink->raw.frameRateFractionDenom= denom;
               sink->raw.frameRateChanged= TRUE;
            }
         }
         if ( (sink->raw.frameRate == 0.0) && (sink->raw.frameRateFractionDenom == 0) )
         {
            sink->raw.frameRateFractionDenom= 1;
            sink->raw.frameRateChanged= TRUE;
         }
         sink->raw.pixelAspectRatio= 1.0;
         if ( gst_structure_get_fraction( structure, "pixel-aspect-ratio", &num, &denom ) )
         {
            if ( (num <= 0) || (denom <= 0))
            {
               num= denom= 1;
            }
            sink->raw.pixelAspectRatio= (double)num/(double)denom;
            sink->raw.havePixelAspectRatio= TRUE;
            sink->raw.pixelAspectRatioChanged= TRUE;
         }
         if ( gst_structure_get_int( structure, "width", &width ) )
         {
            sink->raw.frameWidth= width;
            sink->srcWidth= width;
         }
         if ( gst_structure_get_int( structure, "height", &height ) )
         {
            sink->raw.frameHeight= height;
            sink->srcHeight= height;
         }
         format= gst_structure_get_string( structure, "format" );
         if ( format )
         {
            int len= strlen(format);
            if ( (len == 4) && !strncmp( format, "NV12", len) )
            {
               sink->raw.frameFormatStream= DRM_FORMAT_NV12;
            }
            else if ( (len == 4) && !strncmp( format, "NV21", len) )
            {
               sink->raw.frameFormatStream= DRM_FORMAT_NV21;
            }
            else if ( (len == 4) && !strncmp( format, "I420", len) )
            {
               sink->raw.frameFormatStream= DRM_FORMAT_YUV420;
            }
            else
            {
               g_print("format (%s) not supported\n", format);
               result= FALSE;
            }
         }
      }
   }

   return result;
}

void gst_westeros_sink_raw_set_startPTS( GstWesterosSink *sink, gint64 pts )
{
   WESTEROS_UNUSED(sink);
   WESTEROS_UNUSED(pts);
}

void gst_westeros_sink_raw_render( GstWesterosSink *sink, GstBuffer *buffer )
{
   gboolean flushStarted;
   gboolean haveHardware;
   LOCK(sink);
   haveHardware= sink->raw.haveHardware;
   UNLOCK(sink);
   bool isDmaBuf= false;
   #ifdef USE_GST_ALLOCATORS
   GstMemory *mem;

   mem= gst_buffer_peek_memory( buffer, 0 );
   if ( gst_is_dmabuf_memory(mem) )
   {
      isDmaBuf= true;
   }
   #endif

   if ( !haveHardware )
   {
      return;
   }

   if ( sink->display )
   {
      if ( sink->raw.dispatchThread == NULL )
      {
         sink->raw.quitDispatchThread= FALSE;
         GST_DEBUG_OBJECT(sink, "starting westeros_sink_dispatch thread");
         sink->raw.dispatchThread= g_thread_new("westerossinkDSP", wstDispatchThread, sink);
      }
   }

   if ( sink->raw.eosDetectionThread == NULL )
   {
      sink->raw.videoPlaying= TRUE;
      sink->raw.quitEOSDetectionThread= FALSE;
      GST_DEBUG_OBJECT(sink, "starting westeros_sink_eos thread");
      sink->raw.eosDetectionThread= g_thread_new("westerossinkEOS", wstEOSDetectionThread, sink);
   }

   GST_BASE_SINK_PREROLL_UNLOCK(GST_BASE_SINK(sink));
   while ( sink->raw.videoPaused )
   {
      WstRawVideoClientConnection *conn;
      bool active= true;
      usleep( 1000 );
      LOCK(sink);
      conn= sink->raw.conn;
      if ( conn )
      {
         LOCK_CONN(conn);
      }
      UNLOCK(sink);
      if ( conn )
      {
         wstProcessMessagesVideoClientConnection( conn );
         UNLOCK_CONN(conn);
      }
      LOCK(sink);
      if ( sink->flushStarted || !sink->videoStarted )
      {
         active= false;
      }
      UNLOCK(sink);
      if ( !active )
      {
         GST_BASE_SINK_PREROLL_LOCK(GST_BASE_SINK(sink));
         return;
      }
   }
   GST_BASE_SINK_PREROLL_LOCK(GST_BASE_SINK(sink));

   if ( sink->raw.expectDummyBuffers && !isDmaBuf )
   {
      gint64 frameTime= GST_BUFFER_PTS(buffer);
      gint64 firstNano= ((sink->firstPTS/90LL)*GST_MSECOND)+((sink->firstPTS%90LL)*GST_MSECOND/90LL);
      sink->position= sink->positionSegmentStart + frameTime - firstNano;
      sink->currentPTS= frameTime / (GST_SECOND/90000LL);
      GST_LOG("gst_westeros_sink_raw_render: dummy buffer %p, timestamp: %lld", buffer, GST_BUFFER_PTS(buffer) );
      if ( !sink->raw.conn && (sink->raw.frameOutCount == 0))
      {
         LOCK(sink);
         sink->raw.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
         UNLOCK(sink);
      }
      if ( (sink->raw.frameInCount == 0) && sink->raw.captureEnabled && sink->raw.useTunnelled )
      {
         gst_westeros_sink_raw_set_video_path( sink, true );
      }
      LOCK(sink);
      ++sink->raw.frameInCount;
      ++sink->raw.frameOutCount;
      UNLOCK(sink);
      if ( sink->raw.framesBeforeHideGfx )
      {
         if ( --sink->raw.framesBeforeHideGfx == 0 )
         {
            wl_surface_attach( sink->surface, 0, sink->windowX, sink->windowY );
            wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
            wl_surface_commit( sink->surface );
            wl_display_flush(sink->display);
            wl_display_dispatch_queue_pending(sink->display, sink->queue);
         }
      }
      return;
   }

   if ( !sink->flushStarted )
   {
      gint64 nanoTime;
      gint64 duration;
      int rc, buffIndex= -1;
      int inSize= 0, offset, avail, copylen;
      unsigned char *inData= 0;
      WstDrmBuffer *drmBuff= 0;
      bool importedBuffer= false;
      #ifdef USE_GST1
      GstMapInfo map;
      #endif
      #ifdef USE_GST_ALLOCATORS
      GstMemory *mem;

      mem= gst_buffer_peek_memory( buffer, 0 );
      if ( gst_is_dmabuf_memory(mem) )
      {
         GST_DEBUG("using dma-buf for input");
         drmBuff= drmImportBuffer( sink, buffer );
         if ( drmBuff )
         {
            inSize= drmBuff->size[0] + drmBuff->size[1];
            GST_LOG("gst_westeros_sink_raw_render: buffer %p, len %d timestamp: %lld", buffer, inSize, GST_BUFFER_PTS(buffer) );
            importedBuffer= true;
         }
      }
      #endif

      if ( !importedBuffer )
      {
         #ifdef USE_GST1
         gst_buffer_map(buffer, &map, (GstMapFlags)GST_MAP_READ);
         inSize= map.size;
         inData= map.data;
         #else
         inSize= (int)GST_BUFFER_SIZE(buffer);
         inData= GST_BUFFER_DATA(buffer);
         #endif

         GST_LOG("gst_westeros_sink_raw_render: buffer %p, len %d timestamp: %lld", buffer, inSize, GST_BUFFER_PTS(buffer) );

         drmBuff= drmGetBuffer( sink, sink->raw.frameWidth, sink->raw.frameHeight );
      }

      LOCK(sink);
      flushStarted= sink->flushStarted;
      UNLOCK(sink);

      if ( flushStarted )
      {
         if ( drmBuff )
         {
            drmReleaseBuffer( sink, drmBuff->buffIndex );
         }
      }
      else
      {
         ++sink->raw.frameInCount;

         #ifdef USE_GST_AFD
         wstSetAFDInfo( sink, buffer );
         #endif

         if ( GST_BUFFER_PTS_IS_VALID(buffer) )
         {
            guint64 prevPTS;

            nanoTime= GST_BUFFER_PTS(buffer);
            duration= GST_BUFFER_DURATION(buffer);
            if ( !GST_CLOCK_TIME_IS_VALID(duration) )
            {
               duration= 0;
            }
            {
               guint64 gstNow= getGstClockTime(sink);
               if ( gstNow <= nanoTime )
                  FRAME("in: frame PTS %lld gst clock %lld: lead time %lld us", nanoTime, gstNow, (nanoTime-gstNow)/1000LL);
               else
                  FRAME("in: frame PTS %lld gst clock %lld: lead time %lld us", nanoTime, gstNow, (gstNow-nanoTime)/1000LL);
            }
            LOCK(sink)
            if ( nanoTime+duration >= sink->segment.start )
            {
               if ( sink->prevPositionSegmentStart == 0xFFFFFFFFFFFFFFFFLL )
               {
                  sink->raw.currentInputPTS= 0;
               }
               prevPTS= sink->raw.currentInputPTS;
               sink->raw.currentInputPTS= ((nanoTime / GST_SECOND) * 90000)+(((nanoTime % GST_SECOND) * 90000) / GST_SECOND);
               if (sink->prevPositionSegmentStart != sink->positionSegmentStart)
               {
                  sink->firstPTS= sink->raw.currentInputPTS;
                  sink->prevPositionSegmentStart = sink->positionSegmentStart;
                  GST_DEBUG("SegmentStart changed! Updating first PTS to %lld ", sink->firstPTS);
               }
               if ( sink->raw.currentInputPTS != 0 || sink->raw.frameInCount == 0 )
               {
                  if ( (sink->raw.currentInputPTS < sink->firstPTS) && (sink->raw.currentInputPTS > 90000) )
                  {
                     /* If we have hit a discontinuity that doesn't look like rollover, then
                        treat this as the case of looping a short clip.  Adjust our firstPTS
                        to keep our running time correct. */
                     sink->firstPTS= sink->firstPTS-(prevPTS-sink->raw.currentInputPTS);
                  }
               }
            }
            UNLOCK(sink);
         }

         if ( inSize )
         {
            if ( drmBuff )
            {
               if ( !sink->videoStarted )
               {
                  sink->videoStarted= TRUE;
                  wstSetSessionInfo( sink );
               }

               buffIndex= drmBuff->buffIndex;

               if ( !importedBuffer )
               {
                  unsigned char *data;
                  unsigned char *Y, *U, *V;
                  int Ystride, Ustride, Vstride;
                  #ifdef USE_GST_VIDEO
                  GstVideoMeta *meta= gst_buffer_get_video_meta(buffer);
                  #endif

                  switch( sink->raw.frameFormatStream )
                  {
                     case DRM_FORMAT_NV12:
                     case DRM_FORMAT_NV21:
                        sink->raw.frameFormatOut= sink->raw.frameFormatStream;
                        Y= inData;
                        #ifdef USE_GST_VIDEO
                        if ( meta )
                        {
                           Ystride= meta->stride[0];
                           Ustride= meta->stride[1];
                        }
                        else
                        #endif
                        {
                           Ystride= ((sink->raw.frameWidth + 3) & ~3);
                           Ustride= Ystride;
                        }
                        Vstride= 0;
                        U= Y + Ystride*sink->raw.frameHeight;
                        V= 0;
                        break;
                     case DRM_FORMAT_YUV420:
                        sink->raw.frameFormatOut= DRM_FORMAT_NV12;
                        Y= inData;
                        #ifdef USE_GST_VIDEO
                        if ( meta )
                        {
                           Ystride= meta->stride[0];
                           Ustride= meta->stride[1];
                           Vstride= meta->stride[2];
                        }
                        else
                        #endif
                        {
                           Ystride= ((sink->raw.frameWidth + 3) & ~3);
                           Ustride= Ystride/2;
                           Vstride= Ystride/2;
                        }
                        U= Y + Ystride*sink->raw.frameHeight;
                        V= U + Ustride*sink->raw.frameHeight/2;
                        break;
                     default:
                        Y= U= V= 0;
                        break;
                  }

                  if ( Y )
                  {
                     data= (unsigned char*)mmap( NULL, drmBuff->size[0], PROT_READ | PROT_WRITE, MAP_SHARED, sink->raw.drmFd, drmBuff->offset[0] );
                     if ( data )
                     {
                        int row;
                        unsigned char *destRow= data;
                        unsigned char *srcYRow= Y;
                        for( row= 0; row < sink->raw.frameHeight; ++row )
                        {
                           memcpy( destRow, srcYRow, Ystride );
                           destRow += drmBuff->pitch[0];
                           srcYRow += Ystride;
                        }
                        munmap( data, drmBuff->size[0] );
                     }
                     if ( U && !V )
                     {
                        data= (unsigned char*)mmap( NULL, drmBuff->size[1], PROT_READ | PROT_WRITE, MAP_SHARED, sink->raw.drmFd, drmBuff->offset[1] );
                        if ( data )
                        {
                           int row;
                           unsigned char *destRow= data;
                           unsigned char *srcURow= U;
                           for( row= 0; row < sink->raw.frameHeight; row += 2 )
                           {
                              memcpy( destRow, srcURow, Ustride );
                              destRow += drmBuff->pitch[1];
                              srcURow += Ustride;
                           }
                           munmap( data, drmBuff->size[1] );
                        }
                     }
                     if ( U && V )
                     {
                        int bi;
                        int bufferUOffset;
                        #ifdef USE_SINGLE_BUFFER_NV12
                        bi= 0;
                        bufferUOffset= Ystride*sink->raw.frameHeight;
                        #else
                        bi= 1;
                        bufferUOffset= 0;
                        #endif
                        data= (unsigned char*)mmap( NULL, drmBuff->size[bi], PROT_READ | PROT_WRITE, MAP_SHARED, sink->raw.drmFd, drmBuff->offset[bi] );
                        if ( data )
                        {
                           int row, col;
                           unsigned char *dest, *destRow= data + bufferUOffset;
                           unsigned char *srcU, *srcURow= U;
                           unsigned char *srcV, *srcVRow= V;
                           for( row= 0; row < sink->raw.frameHeight; row += 2 )
                           {
                              dest= destRow;
                              srcU= srcURow;
                              srcV= srcVRow;
                              for( col= 0; col < sink->raw.frameWidth; col += 2 )
                              {
                                 *dest++= *srcU++;
                                 *dest++= *srcV++;
                              }
                              destRow += drmBuff->pitch[bi];
                              srcURow += Ustride;
                              srcVRow += Vstride;
                           }
                           munmap( data, drmBuff->size[bi] );
                        }
                     }
                  }
               }

               if ( !sink->raw.conn && (sink->raw.frameOutCount == 0))
               {
                  LOCK(sink);
                  sink->raw.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
                  UNLOCK(sink);
               }

               drmBuff->frameTime= ((GST_BUFFER_PTS(buffer) + 500LL) / 1000LL);

               if ( !sink->raw.conn )
               {
                  /* If we are not connected to a video server, set position here */
                  gint64 frameTime= GST_BUFFER_PTS(buffer);
                  gint64 firstNano= ((sink->firstPTS/90LL)*GST_MSECOND)+((sink->firstPTS%90LL)*GST_MSECOND/90LL);
                  sink->position= sink->positionSegmentStart + frameTime - firstNano;
                  sink->currentPTS= frameTime / (GST_SECOND/90000LL);
                  if ( sink->timeCodePresent && sink->enableTimeCodeSignal )
                  {
                     sink->timeCodePresent( sink, sink->position, g_signals[SIGNAL_TIMECODE] );
                  }
               }

               if ( sink->raw.enableTextureSignal )
               {
                  int fd0, l0, s0, fd1, l1, fd2, s1, l2, s2;
                  void *p0, *p1, *p2;

                  fd0= drmBuff->fd[0];
                  fd1= drmBuff->fd[1];
                  fd2= -1;
                  s0= drmBuff->pitch[0];
                  s1= drmBuff->pitch[1];
                  s2= 0;
                  l0= drmBuff->size[0];
                  l1= drmBuff->size[1];
                  l2= 0;
                  p0= 0;
                  p1= 0;
                  p2= 0;

                  g_signal_emit( G_OBJECT(sink),
                                 g_signals[SIGNAL_NEWTEXTURE],
                                 0,
                                 sink->raw.frameFormatOut,
                                 sink->raw.frameWidth,
                                 sink->raw.frameHeight,
                                 fd0, l0, s0, p0,
                                 fd1, l1, s1, p1,
                                 fd2, l2, s2, p2
                               );
               }
               else if ( sink->raw.captureEnabled && sink->raw.sb && sink->show )
               {
                  bufferInfo *binfo;
                  binfo= (bufferInfo*)malloc( sizeof(bufferInfo) );
                  if ( binfo )
                  {
                     struct wl_buffer *wlbuff;
                     int fd0, fd1, fd2;
                     int stride0, stride1;
                     int offset1= 0;
                     int pixelFormat;
                     fd0= drmBuff->fd[0];
                     fd1= drmBuff->fd[1];
                     fd2= fd0;
                     stride0= drmBuff->pitch[0];
                     stride1= drmBuff->pitch[1];
                     if ( fd1 < 0 )
                     {
                        fd1= fd0;
                        stride1= stride0;
                        offset1= stride0*drmBuff->height;
                     }
                     pixelFormat= (sink->raw.frameFormatOut == DRM_FORMAT_NV12) ? WL_SB_FORMAT_NV12 : WL_SB_FORMAT_NV21;

                     binfo->sink= sink;
                     binfo->buffIndex= buffIndex;

                     wlbuff= wl_sb_create_planar_buffer_fd2( sink->raw.sb,
                                                             fd0,
                                                             fd1,
                                                             fd2,
                                                             drmBuff->width,
                                                             drmBuff->height,
                                                             pixelFormat,
                                                             0, /* offset0 */
                                                             offset1, /* offset1 */
                                                             0, /* offset2 */
                                                             stride0, /* stride0 */
                                                             stride1, /* stride1 */
                                                             0  /* stride2 */
                                                           );
                     if ( wlbuff )
                     {
                        wl_buffer_add_listener( wlbuff, &wl_buffer_listener, binfo );
                        wl_surface_attach( sink->surface, wlbuff, sink->windowX, sink->windowY );
                        wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
                        wl_surface_commit( sink->surface );
                        wl_display_flush(sink->display);

                        drmLockBuffer( sink, buffIndex );

                        /* Advance any frames sent to video server towards requeueing to decoder */
                        sink->raw.resubFd= sink->raw.prevFrame2Fd;
                        sink->raw.prevFrame2Fd=sink->raw.prevFrame1Fd;
                        sink->raw.prevFrame1Fd= sink->raw.nextFrameFd;
                        sink->raw.nextFrameFd= -1;

                        if ( sink->raw.framesBeforeHideVideo )
                        {
                           if ( --sink->raw.framesBeforeHideVideo == 0 )
                           {
                              wstSendHideVideoClientConnection( sink->raw.conn, true );
                           }
                        }
                     }
                     else
                     {
                        free( binfo );
                     }
                  }
               }
               if ( sink->raw.conn )
               {
                  if ( sink->raw.expectDummyBuffers )
                  {
                     buffIndex= -1;
                  }
                  else
                  {
                     if ( sink->raw.showChanged )
                     {
                        sink->raw.showChanged= FALSE;
                        if ( !sink->raw.captureEnabled )
                        {
                           wstSendHideVideoClientConnection( sink->raw.conn, !sink->show );
                        }
                     }
                     if ( sink->raw.frameRateChanged )
                     {
                        sink->raw.frameRateChanged= FALSE;
                        wstSendRateVideoClientConnection( sink->raw.conn );
                     }
                     sink->raw.resubFd= sink->raw.prevFrame2Fd;
                     sink->raw.prevFrame2Fd= sink->raw.prevFrame1Fd;
                     sink->raw.prevFrame1Fd= sink->raw.nextFrameFd;
                     sink->raw.nextFrameFd= sink->raw.drmBuffer[buffIndex].fd[0];

                     if ( wstSendFrameVideoClientConnection( sink->raw.conn, buffIndex ) )
                     {
                        buffIndex= -1;
                     }

                     if ( sink->raw.framesBeforeHideGfx )
                     {
                        if ( --sink->raw.framesBeforeHideGfx == 0 )
                        {
                           wl_surface_attach( sink->surface, 0, sink->windowX, sink->windowY );
                           wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
                           wl_surface_commit( sink->surface );
                           wl_display_flush(sink->display);
                           wl_display_dispatch_queue_pending(sink->display, sink->queue);
                           if ( sink->show )
                           {
                              wstSendHideVideoClientConnection( sink->raw.conn, false );
                           }
                        }
                     }
                  }
               }
            }
            if ( buffIndex != -1 )
            {
               drmReleaseBuffer( sink, buffIndex );
            }
         }
         LOCK(sink);
         ++sink->raw.frameOutCount;
         UNLOCK(sink);
      }

      if ( !importedBuffer )
      {
         #ifdef USE_GST1
         gst_buffer_unmap( buffer, &map);
         #endif
      }
   }
}

void gst_westeros_sink_raw_flush( GstWesterosSink *sink )
{
   GST_DEBUG("gst_westeros_sink_raw_flush");
   if ( sink->videoStarted )
   {
      LOCK(sink);
      sink->videoStarted= FALSE;
      UNLOCK(sink);
      wstSendFlushVideoClientConnection( sink->raw.conn );
      sink->startAfterCaps= TRUE;
      sink->raw.prevFrameTimeGfx= 0;
      sink->raw.prevFramePTSGfx= 0;
      sink->raw.prevFrame1Fd= -1;
      sink->raw.prevFrame2Fd= -1;
      sink->raw.nextFrameFd= -1;
   }
   LOCK(sink);
   sink->raw.frameInCount= 0;
   sink->raw.frameOutCount= 0;
   sink->raw.frameDisplayCount= 0;
   sink->raw.numDropped= 0;
   #ifdef USE_GST_AFD
   wstFlushAFDInfo( sink );
   #endif
   UNLOCK(sink);
}

gboolean gst_westeros_sink_raw_start_video( GstWesterosSink *sink )
{
   WESTEROS_UNUSED(sink);
   return TRUE;
}

void gst_westeros_sink_raw_eos_event( GstWesterosSink *sink )
{
   WESTEROS_UNUSED(sink);
}

void gst_westeros_sink_raw_set_video_path( GstWesterosSink *sink, bool useGfxPath )
{
   if ( useGfxPath && !sink->raw.captureEnabled )
   {
      sink->raw.captureEnabled= TRUE;

      sink->raw.framesBeforeHideVideo= sink->raw.hideVideoFramesDelay;
   }
   else if ( !useGfxPath && sink->raw.captureEnabled )
   {
      sink->raw.captureEnabled= FALSE;
      sink->raw.prevFrame1Fd= -1;
      sink->raw.prevFrame2Fd= -1;
      sink->raw.nextFrameFd= -1;
      sink->raw.framesBeforeHideGfx= sink->raw.hideGfxFramesDelay;
   }
   if ( sink->raw.useTunnelled )
   {
      GstPad *pad= GST_BASE_SINK(sink)->sinkpad;
      if ( pad )
      {
         GstStructure *structure;
         int vx, vy, vw, vh;
         guint gfxpath= sink->raw.captureEnabled ? 1 : 0;
         if ( sink->raw.captureEnabled )
         {
            vx= 0;
            vy= 0;
            vw= 1;
            vh= 1;
         }
         else
         {
            vx= sink->raw.videoX;
            vy= sink->raw.videoY;
            vw= sink->raw.videoWidth;
            vh= sink->raw.videoHeight;
         }
         structure= gst_structure_new("westeros-raw-rectangle",
                                      "res-width", G_TYPE_UINT, sink->displayWidth,
                                      "res-height", G_TYPE_UINT, sink->displayHeight,
                                      "rectx", G_TYPE_INT, vx,
                                      "recty", G_TYPE_INT, vy,
                                      "rectw", G_TYPE_INT, vw,
                                      "recth", G_TYPE_INT, vh,
                                       NULL );
         if ( structure )
         {
            GST_DEBUG("push westeros-raw-rectangle");
            gst_pad_push_event( pad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure));
         }
         structure= gst_structure_new("westeros-raw-path",
                                      "gfxpath", G_TYPE_UINT, gfxpath,
                                       NULL );
         if ( structure )
         {
            GST_DEBUG("push westeros-raw-path");
            gst_pad_push_event( pad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure));
         }
      }
   }
   if ( needBounds(sink) && sink->vpcSurface )
   {
      /* Use nominal display size provided to us by
       * the compositor to calculate the video bounds
       * we should use when we transition to graphics path.
       * Save and restore current HW video rectangle. */
      int vx, vy, vw, vh;
      int tx, ty, tw, th;
      tx= sink->raw.videoX;
      ty= sink->raw.videoY;
      tw= sink->raw.videoWidth;
      th= sink->raw.videoHeight;
      sink->raw.videoX= sink->windowX;
      sink->raw.videoY= sink->windowY;
      sink->raw.videoWidth= sink->windowWidth;
      sink->raw.videoHeight= sink->windowHeight;

      wstGetVideoBounds( sink, &vx, &vy, &vw, &vh );
      wstSetTextureCrop( sink, vx, vy, vw, vh );

      sink->raw.videoX= tx;
      sink->raw.videoY= ty;
      sink->raw.videoWidth= tw;
      sink->raw.videoHeight= th;
   }
}

void gst_westeros_sink_raw_update_video_position( GstWesterosSink *sink )
{
   bool needUpdate= true;
   int vx, vy, vw, vh;
   vx= sink->raw.videoX;
   vy= sink->raw.videoY;
   vw= sink->raw.videoWidth;
   vh= sink->raw.videoHeight;

   if ( sink->windowSizeOverride )
   {
      sink->raw.videoX= ((sink->windowX*sink->scaleXNum)/sink->scaleXDenom) + sink->transX;
      sink->raw.videoY= ((sink->windowY*sink->scaleYNum)/sink->scaleYDenom) + sink->transY;
      sink->raw.videoWidth= (sink->windowWidth*sink->scaleXNum)/sink->scaleXDenom;
      sink->raw.videoHeight= (sink->windowHeight*sink->scaleYNum)/sink->scaleYDenom;
   }
   else
   {
      sink->raw.videoX= sink->transX;
      sink->raw.videoY= sink->transY;
      sink->raw.videoWidth= (sink->outputWidth*sink->scaleXNum)/sink->scaleXDenom;
      sink->raw.videoHeight= (sink->outputHeight*sink->scaleYNum)/sink->scaleYDenom;
   }

   if ( (vx == sink->raw.videoX) && (vy == sink->raw.videoY) &&
        (vw == sink->raw.videoWidth) && (vh == sink->raw.videoHeight) )
   {
      needUpdate= false;
   }

   if ( !sink->raw.captureEnabled && needUpdate )
   {
      /* Send a buffer to compositor to update hole punch geometry */
      if ( sink->raw.sb )
      {
         struct wl_buffer *buff;

         buff= wl_sb_create_buffer( sink->raw.sb,
                                    0,
                                    sink->windowWidth,
                                    sink->windowHeight,
                                    sink->windowWidth*4,
                                    WL_SB_FORMAT_ARGB8888 );
         wl_surface_attach( sink->surface, buff, sink->windowX, sink->windowY );
         wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
         wl_surface_commit( sink->surface );
      }
      if ( sink->raw.videoPaused )
      {
         wstSendRectVideoClientConnection(sink->raw.conn);
      }
      if ( sink->raw.useTunnelled )
      {
         GstPad *pad= GST_BASE_SINK(sink)->sinkpad;
         if ( pad )
         {
            GstStructure *structure;
            structure= gst_structure_new("westeros-raw-rectangle",
                                         "res-width", G_TYPE_UINT, sink->displayWidth,
                                         "res-height", G_TYPE_UINT, sink->displayHeight,
                                         "rectx", G_TYPE_INT, sink->raw.videoX,
                                         "recty", G_TYPE_INT, sink->raw.videoY,
                                         "rectw", G_TYPE_INT, sink->raw.videoWidth,
                                         "recth", G_TYPE_INT, sink->raw.videoHeight,
                                          NULL );
            if ( structure )
            {
               GST_DEBUG("push westeros-raw-rectangle");
               gst_pad_push_event( pad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure));
            }
         }
      }
   }
}

gboolean gst_westeros_sink_raw_query( GstWesterosSink *sink, GstQuery *query )
{
   return FALSE;
}

static void wstSinkRawStopVideo( GstWesterosSink *sink )
{
   LOCK(sink);
   sink->videoStarted= FALSE;
   if ( sink->raw.conn )
   {
      wstDestroyVideoClientConnection( sink->raw.conn );
      sink->raw.conn= 0;
   }
   if ( sink->raw.eosDetectionThread || sink->raw.dispatchThread )
   {
      sink->raw.quitEOSDetectionThread= TRUE;
      sink->raw.quitDispatchThread= TRUE;
      if ( sink->display )
      {
         int fd= wl_display_get_fd( sink->display );
         if ( fd >= 0 )
         {
            shutdown( fd, SHUT_RDWR );
         }
      }
   }
   drmUnlockAllBuffers( sink );
   UNLOCK(sink);

   sink->raw.prevFrame1Fd= -1;
   sink->raw.prevFrame2Fd= -1;
   sink->raw.nextFrameFd= -1;
   sink->raw.frameWidth= -1;
   sink->raw.frameHeight= -1;
   sink->raw.frameRate= 0.0;
   sink->raw.frameRateFractionNum= 0;
   sink->raw.frameRateFractionDenom= 0;
   sink->raw.pixelAspectRatio= 1.0;
   sink->raw.havePixelAspectRatio= FALSE;
   sink->raw.syncType= -1;
   sink->raw.emitFirstFrameSignal= FALSE;
   sink->raw.emitUnderflowSignal= FALSE;

   LOCK(sink);
   sink->videoStarted= FALSE;
   #ifdef USE_GST_AFD
   wstFlushAFDInfo( sink);
   #endif
   UNLOCK(sink);

   if ( sink->raw.eosDetectionThread )
   {
      sink->raw.quitEOSDetectionThread= TRUE;
      g_thread_join( sink->raw.eosDetectionThread );
      sink->raw.eosDetectionThread= NULL;
   }

   if ( sink->raw.dispatchThread )
   {
      sink->raw.quitDispatchThread= TRUE;
      g_thread_join( sink->raw.dispatchThread );
      sink->raw.dispatchThread= NULL;
   }

   #ifdef USE_GENERIC_AVSYNC
   if ( sink->raw.avsctx )
   {
      wstDestroyAVSyncCtx( sink, sink->raw.avsctx );
      sink->raw.avsctx= 0;
   }
   #endif

   if ( sink->raw.sb )
   {
      wl_sb_destroy( sink->raw.sb );
      sink->raw.sb= 0;
   }

   drmTerm( sink );
}

static void wstGetVideoBounds( GstWesterosSink *sink, int *x, int *y, int *w, int *h )
{
   int vx, vy, vw, vh;
   int frameWidth, frameHeight;
   int zoomMode;;
   double contentWidth, contentHeight;
   double roix, roiy, roiw, roih;
   double arf, ard;
   double hfactor= 1.0, vfactor= 1.0;
   vx= sink->raw.videoX;
   vy= sink->raw.videoY;
   vw= sink->raw.videoWidth;
   vh= sink->raw.videoHeight;
   if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("pixelAspectRatio: %f zoom-mode %d overscan-size %d", sink->raw.pixelAspectRatio, sink->raw.zoomMode, sink->raw.overscanSize );
   frameWidth= sink->raw.frameWidth;
   frameHeight= sink->raw.frameHeight;
   contentWidth= frameWidth*sink->raw.pixelAspectRatio;
   contentHeight= frameHeight;
   if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("frame %dx%d contentWidth: %f contentHeight %f", frameWidth, frameHeight, contentWidth, contentHeight );
   ard= (double)sink->raw.videoWidth/(double)sink->raw.videoHeight;
   arf= (double)contentWidth/(double)contentHeight;

   /* Establish region of interest */
   roix= 0;
   roiy= 0;
   roiw= contentWidth;
   roih= contentHeight;

   zoomMode= sink->raw.zoomMode;
   if ( !sink->raw.allow4kZoom &&
        ((sink->raw.frameWidth > 1920) || (sink->raw.frameHeight > 1080)) )
   {
      zoomMode= ZOOM_NORMAL;
      if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("4k (%dx%d) force zoom mormal", sink->raw.frameWidth, sink->raw.frameHeight);
   }
   if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("ard %f arf %f", ard, arf);
   switch( zoomMode )
   {
      case ZOOM_NORMAL:
         {
            if ( arf >= ard )
            {
               vw= sink->raw.videoWidth * (1.0+(2.0*sink->raw.overscanSize/100.0));
               vh= (roih * vw) / roiw;
               vx= vx+(sink->raw.videoWidth-vw)/2;
               vy= vy+(sink->raw.videoHeight-vh)/2;
            }
            else
            {
               vh= sink->raw.videoHeight * (1.0+(2.0*sink->raw.overscanSize/100.0));
               vw= (roiw * vh) / roih;
               vx= vx+(sink->raw.videoWidth-vw)/2;
               vy= vy+(sink->raw.videoHeight-vh)/2;
            }
         }
         break;
      case ZOOM_NONE:
      case ZOOM_DIRECT:
         {
            if ( arf >= ard )
            {
               vh= (contentHeight * sink->raw.videoWidth) / contentWidth;
               vy= vy+(sink->raw.videoHeight-vh)/2;
            }
            else
            {
               vw= (contentWidth * sink->raw.videoHeight) / contentHeight;
               vx= vx+(sink->raw.videoWidth-vw)/2;
            }
         }
         break;
      case ZOOM_16_9_STRETCH:
         {
            if ( wstApproxEqual(arf, ard) && wstApproxEqual(arf, 1.777) )
            {
               /* For 16:9 content on a 16:9 display, stretch as though 4:3 */
               hfactor= 4.0/3.0;
               if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("stretch apply vfactor %f hfactor %f", vfactor, hfactor);
            }
            vh= sink->raw.videoHeight * (1.0+(2.0*sink->raw.overscanSize/100.0));
            vw= vh*16/9;
            vx= vx+(sink->raw.videoWidth-vw)/2;
            vy= vy+(sink->raw.videoHeight-vh)/2;
         }
         break;
      case ZOOM_4_3_PILLARBOX:
         {
            vh= sink->raw.videoHeight * (1.0+(2.0*sink->raw.overscanSize/100.0));
            vw= vh*4/3;
            vx= vx+(sink->raw.videoWidth-vw)/2;
            vy= vy+(sink->raw.videoHeight-vh)/2;
         }
         break;
      case ZOOM_ZOOM:
         {
            #ifdef USE_GST_AFD
            /* Adjust region of interest based on AFD+Bars */
            if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("afd %d haveBar %d isLetterbox %d d1 %d d2 %d", sink->raw.afdActive.afd, sink->raw.afdActive.haveBar,
                                                                sink->raw.afdActive.isLetterbox, sink->raw.afdActive.d1, sink->raw.afdActive.d2 );
            switch ( sink->raw.afdActive.afd )
            {
               case GST_VIDEO_AFD_4_3_FULL_16_9_FULL: /* AFD 8 (1000) */
                  /* 16:9 and 4:3 content are full frame */
                  break;
               case GST_VIDEO_AFD_14_9_LETTER_14_9_PILLAR: /* AFD 11 (1011) */
                  /* 4:3 contains 14:9 letterbox vertically centered */
                  /* 16:9 contains 14:9 pillarbox horizontally centered */
                  break;
               case GST_VIDEO_AFD_4_3_FULL_14_9_CENTER: /* AFD 13 (1101) */
                  /* 4:3 content is full frame */
                  /* 16:9 contains 4:3 pillarbox */
                  break;
               case GST_VIDEO_AFD_GREATER_THAN_16_9: /* AFD 4 (0100) */
                  /* 4:3 contains letterbox image with aspect ratio > 16:9 vertically centered */
                  /* 16:9 contains letterbox image with aspect ratio > 16:9 */
                  /* should be accompanied by bar data */
                  if ( sink->raw.afdActive.haveBar )
                  {
                     int activeHeight= roih-sink->raw.afdActive.d1;
                     if ( activeHeight > 0 )
                     {
                        /* ignore bar data for now
                        hfactor= 1.0;
                        vfactor= roiw/activeHeight;
                        arf= ard;
                        */
                     }
                  }
                  break;
               case GST_VIDEO_AFD_4_3_FULL_4_3_PILLAR: /* AFD 9 (1001) */
                  /* 4:3 content is full frame */
                  /* 16:9 content is 4:3 roi horizontally centered */
                  if ( arf > (4.0/3.0) )
                  {
                     hfactor= 1.0;
                     vfactor= 1.0;
                     arf= ard;
                  }
                  break;
               case GST_VIDEO_AFD_16_9_LETTER_16_9_FULL: /* AFD 10 (1010) */
               case GST_VIDEO_AFD_16_9_LETTER_14_9_CENTER: /* AFD 14 (1110) */
               case GST_VIDEO_AFD_16_9_LETTER_4_3_CENTER: /* AFD 15 (1111) */
                  /* 4:3 content has 16:9 letterbox roi vertically centered */
                  /* 16:9 content is full frame 16:9 */
                  if ( arf < (16.0/9.0) )
                  {
                     hfactor= 1.0;
                     vfactor= 4.0/3.0;
                     arf= ard;
                  }
                  break;
               default:
                  break;
            }
            #endif

            if ( (arf >= ard) || wstApproxEqual(arf, ard) )
            {
               if ( wstApproxEqual(arf, ard) && wstApproxEqual( arf, 1.777) )
               {
                  /* For 16:9 content on a 16:9 display, enlarge as though 4:3 */
                  vfactor= 4.0/3.0;
                  hfactor= 1.0;
                  if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("zoom apply vfactor %f hfactor %f", vfactor, hfactor);
               }
               vh= sink->raw.videoHeight * vfactor * (1.0+(2.0*sink->raw.overscanSize/100.0));
               vw= (roiw * vh) * hfactor / roih;
               vx= vx+(sink->raw.videoWidth-vw)/2;
               vy= vy+(sink->raw.videoHeight-vh)/2;
            }
            else
            {
               vw= sink->raw.videoWidth * (1.0+(2.0*sink->raw.overscanSize/100.0));
               vh= (roih * vw) / roiw;
               vx= vx+(sink->raw.videoWidth-vw)/2;
               vy= vy+(sink->raw.videoHeight-vh)/2;
            }
         }
         break;
   }
   if ( sink->raw.pixelAspectRatioChanged ) GST_DEBUG("vrect %d, %d, %d, %d", vx, vy, vw, vh);
   if ( sink->raw.pixelAspectRatioChanged )
   {
      if ( sink->display && sink->vpcSurface )
      {
         if ( sink->raw.captureEnabled || sink->raw.framesBeforeHideGfx )
         {
            wl_vpc_surface_set_geometry( sink->vpcSurface, vx, vy, vw, vh );
         }
         else
         {
            wl_vpc_surface_set_geometry( sink->vpcSurface, sink->windowX, sink->windowY, sink->windowWidth, sink->windowHeight );
         }
         wl_display_flush(sink->display);
      }
   }
   sink->raw.pixelAspectRatioChanged= FALSE;
   *x= vx;
   *y= vy;
   *w= vw;
   *h= vh;
}

static void wstSetTextureCrop( GstWesterosSink *sink, int vx, int vy, int vw, int vh )
{
   GST_DEBUG("wstSetTextureCrop: vx %d vy %d vw %d vh %d window(%d, %d, %d, %d) display(%dx%d)",
             vx, vy, vw, vh, sink->windowX, sink->windowY, sink->windowWidth, sink->windowHeight, sink->displayWidth, sink->displayHeight);
   if ( (sink->displayWidth != -1) && (sink->displayHeight != -1) &&
        ( (vx < 0) || (vx+vw > sink->displayWidth) ||
          (vy < 0) || (vy+vh > sink->displayHeight) ) )
   {
      int cropx, cropy, cropw, croph;
      int wx1, wx2, wy1, wy2;
      cropx= 0;
      cropw= sink->windowWidth;
      cropy= 0;
      croph= sink->windowHeight;
      if ( (vx < sink->windowX) || (vx+vw > sink->windowX+sink->windowWidth) )
      {
         GST_LOG("wstSetTextureCrop: CX1");
         cropx= (sink->windowX-vx)*sink->windowWidth/vw;
         cropw= (sink->windowX+sink->windowWidth-vx)*sink->windowWidth/vw - cropx;
      }
      else if ( vx < 0 )
      {
         GST_LOG("wstSetTextureCrop: CX2");
         cropx= -vx*sink->windowWidth/vw;
         cropw= (vw+vx)*sink->windowWidth/vw;
      }
      else if ( vx+vw > sink->windowWidth )
      {
         GST_LOG("wstSetTextureCrop: CX3");
         cropx= 0;
         cropw= (sink->windowWidth-vx)*sink->windowWidth/vw;
      }

      if ( (vy < sink->windowY) || (vy+vh > sink->windowY+sink->windowHeight) )
      {
         GST_LOG("wstSetTextureCrop: CY1");
         cropy= (sink->windowY-vy)*sink->windowHeight/vh;
         croph= (sink->windowY+sink->windowHeight-vy)*sink->windowHeight/vh - cropy;
      }
      else if ( vy < 0 )
      {
         GST_LOG("wstSetTextureCrop: CY2");
         cropy= -vy*sink->windowHeight/vh;
         croph= (vh+vy)*sink->windowHeight/vh;
      }
      else if ( vy+vh > sink->windowHeight )
      {
         GST_LOG("wstSetTextureCrop: CY3");
         cropy= 0;
         croph= (sink->windowHeight-vy)*sink->windowHeight/vh;
      }

      wx1= vx;
      wx2= vx+vw;
      wy1= vy;
      wy2= vy+vh;
      vx= sink->windowX;
      vy= sink->windowY;
      vw= sink->windowWidth;
      vh= sink->windowHeight;
      if ( (wx1 > vx) && (wx1 > 0) )
      {
         GST_LOG("wstSetTextureCrop: WX1");
         vx= wx1;
      }
      else if ( (wx1 >= vx) && (wx1 < 0) )
      {
         GST_LOG("wstSetTextureCrop: WX2");
         vw += wx1;
         vx= 0;
      }
      else if ( wx2 < vx+vw )
      {
         GST_LOG("wstSetTextureCrop: WX3");
         vw= wx2-vx;
      }
      if ( (wx1 >= 0) && (wx2 > vw) )
      {
         GST_LOG("wstSetTextureCrop: WX4");
         vw= vw-wx1;
      }
      else if ( wx2 < vx+vw )
      {
         GST_LOG("wstSetTextureCrop: WX5");
         vw= wx2-vx;
      }

      if ( (wy1 > vy) && (wy1 > 0) )
      {
         GST_LOG("wstSetTextureCrop: WY1");
         vy= wy1;
      }
      else if ( (wy1 >= vy) && (wy1 < 0) )
      {
         GST_LOG("wstSetTextureCrop: WY2");
         vy= 0;
      }
      else if ( (wy1 < vy) && (wy1 > 0) )
      {
         GST_LOG("wstSetTextureCrop: WY3");
         vh -= wy1;
      }
      if ( (wy1 >= 0) && (wy2 > vh) )
      {
         GST_LOG("wstSetTextureCrop: WY4");
         vh= vh-wy1;
      }
      else if ( wy2 < vy+vh )
      {
         GST_LOG("wstSetTextureCrop: WY5");
         vh= wy2-vy;
      }
      if ( vw < 0 ) vw= 0;
      if ( vh < 0 ) vh= 0;
      cropx= (cropx*WL_VPC_SURFACE_CROP_DENOM)/sink->windowWidth;
      cropy= (cropy*WL_VPC_SURFACE_CROP_DENOM)/sink->windowHeight;
      cropw= (cropw*WL_VPC_SURFACE_CROP_DENOM)/sink->windowWidth;
      croph= (croph*WL_VPC_SURFACE_CROP_DENOM)/sink->windowHeight;
      GST_DEBUG("wstSetTextureCrop: %d, %d, %d, %d - %d, %d, %d, %d\n", vx, vy, vw, vh, cropx, cropy, cropw, croph);
      wl_vpc_surface_set_geometry_with_crop( sink->vpcSurface, vx, vy, vw, vh, cropx, cropy, cropw, croph );
   }
   else
   {
      if ( sink->raw.captureEnabled || sink->raw.framesBeforeHideGfx )
      {
         wl_vpc_surface_set_geometry( sink->vpcSurface, vx, vy, vw, vh );
      }
      else
      {
         wl_vpc_surface_set_geometry( sink->vpcSurface, sink->windowX, sink->windowY, sink->windowWidth, sink->windowHeight );
      }
   }
}

static WstRawVideoClientConnection *wstCreateVideoClientConnection( GstWesterosSink *sink, const char *name )
{
   WstRawVideoClientConnection *conn= 0;
   int rc;
   bool error= true;
   const char *workingDir;
   int pathNameLen, addressSize;

   conn= (WstRawVideoClientConnection*)calloc( 1, sizeof(WstRawVideoClientConnection));
   if ( conn )
   {
      conn->socketFd= -1;
      conn->name= name;
      conn->sink= sink;
      #ifdef GLIB_VERSION_2_32
      g_mutex_init( &conn->mutex );
      #else
      conn->mutex= g_mutex_new();
      #endif

      workingDir= getenv("XDG_RUNTIME_DIR");
      if ( !workingDir )
      {
         GST_ERROR("wstCreateVideoClientConnection: XDG_RUNTIME_DIR is not set");
         goto exit;
      }

      pathNameLen= strlen(workingDir)+strlen("/")+strlen(conn->name)+1;
      if ( pathNameLen > (int)sizeof(conn->addr.sun_path) )
      {
         GST_ERROR("wstCreateVideoClientConnection: name for server unix domain socket is too long: %d versus max %d",
                pathNameLen, (int)sizeof(conn->addr.sun_path) );
         goto exit;
      }

      conn->addr.sun_family= AF_LOCAL;
      strcpy( conn->addr.sun_path, workingDir );
      strcat( conn->addr.sun_path, "/" );
      strcat( conn->addr.sun_path, conn->name );

      conn->socketFd= socket( PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0 );
      if ( conn->socketFd < 0 )
      {
         GST_ERROR("wstCreateVideoClientConnection: unable to open socket: errno %d", errno );
         goto exit;
      }

      addressSize= pathNameLen + offsetof(struct sockaddr_un, sun_path);

      rc= connect(conn->socketFd, (struct sockaddr *)&conn->addr, addressSize );
      if ( rc < 0 )
      {
         GST_ERROR("wstCreateVideoClientConnection: connect failed for socket: errno %d", errno );
         goto exit;
      }

      wstSendResourceVideoClientConnection( conn );

      error= false;
   }

exit:

   if ( error )
   {
      wstDestroyVideoClientConnection( conn );
      conn= 0;
   }

   return conn;
}

static void wstDestroyVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      LOCK_CONN(conn);
      conn->addr.sun_path[0]= '\0';

      if ( conn->socketFd >= 0 )
      {
         close( conn->socketFd );
         conn->socketFd= -1;
      }
      UNLOCK_CONN(conn);

      #ifdef GLIB_VERSION_2_32
      g_mutex_clear( &conn->mutex );
      #else
      g_mutex_free( conn->mutex );
      #endif

      free( conn );
   }
}

static unsigned int getU32( unsigned char *p )
{
   unsigned n;

   n= (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|(p[3]);

   return n;
}

static int putU32( unsigned char *p, unsigned n )
{
   p[0]= (n>>24);
   p[1]= (n>>16);
   p[2]= (n>>8);
   p[3]= (n&0xFF);

   return 4;
}

static gint64 getS64( unsigned char *p )
{
   gint64 n;

   n= ((((gint64)(p[0]))<<56) |
       (((gint64)(p[1]))<<48) |
       (((gint64)(p[2]))<<40) |
       (((gint64)(p[3]))<<32) |
       (((gint64)(p[4]))<<24) |
       (((gint64)(p[5]))<<16) |
       (((gint64)(p[6]))<<8) |
       (p[7]) );

   return n;
}

static int putS64( unsigned char *p,  gint64 n )
{
   p[0]= (((guint64)n)>>56);
   p[1]= (((guint64)n)>>48);
   p[2]= (((guint64)n)>>40);
   p[3]= (((guint64)n)>>32);
   p[4]= (((guint64)n)>>24);
   p[5]= (((guint64)n)>>16);
   p[6]= (((guint64)n)>>8);
   p[7]= (((guint64)n)&0xFF);

   return 8;
}

static void wstSendResourceVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      GstWesterosSink *sink= conn->sink;
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[8];
      int len;
      int sentLen;
      int resourceId= ((sink->resAssignedId >= 0) ? sink->resAssignedId : 0);

      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 5;
      mbody[len++]= 'V';
      len += putU32( &mbody[len], resourceId );

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent resource id to video server");
         FRAME("sent resource id to video server");
      }
   }
}

static void wstSendHideVideoClientConnection( WstRawVideoClientConnection *conn, bool hide )
{
   if ( conn )
   {
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[7];
      int len;
      int sentLen;

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 2;
      mbody[len++]= 'H';
      mbody[len++]= (hide ? 1 : 0);

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent hide %d to video server", hide);
         FRAME("sent hide %d to video server", hide);
      }
      UNLOCK_CONN( conn );
   }
}

static void wstSendSessionInfoVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      GstWesterosSink *sink= conn->sink;
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[13];
      int len;
      int sentLen;
      #ifdef USE_GENERIC_AVSYNC
      struct cmsghdr *cmsg;
      char cmbody[CMSG_SPACE(sizeof(int))];
      int fdToSend= -1;
      #endif

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 6;
      mbody[len++]= 'I';
      mbody[len++]= sink->raw.syncType;
      len += putU32( &mbody[len], conn->sink->raw.sessionId );
      #ifdef USE_GENERIC_AVSYNC
      if ( sink->raw.avsctx )
      {
         fdToSend= fcntl( sink->raw.avsctx->fd, F_DUPFD_CLOEXEC, 0 );
         if ( fdToSend >= 0 )
         {
            int *fd;
            cmsg= (struct cmsghdr*)cmbody;
            cmsg->cmsg_len= CMSG_LEN(sizeof(int));
            cmsg->cmsg_level= SOL_SOCKET;
            cmsg->cmsg_type= SCM_RIGHTS;

            msg.msg_control= cmsg;
            msg.msg_controllen= cmsg->cmsg_len;

            fd= (int*)CMSG_DATA(cmsg);
            fd[0]= fdToSend;

            len += putU32( &mbody[len], sink->raw.avsctx->ctrlSize );
            mbody[2]= (len-3);
         }
         else
         {
            GST_ERROR("wstSendSessionInfoVideoClientConnection: failed to dup avsctx fd");
         }
      }
      #endif

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_DEBUG("sent session info: type %d sessionId %d to video server", sink->raw.syncType, sink->raw.sessionId);
         g_print("sent session info: type %d sessionId %d to video server\n", sink->raw.syncType, sink->raw.sessionId);
      }
      #ifdef USE_GENERIC_AVSYNC
      if ( fdToSend >= 0 )
      {
         close( fdToSend );
      }
      #endif
      UNLOCK_CONN( conn );
   }
}

#if defined USE_AMLOGIC_MESON || defined USE_GENERIC_AVSYNC
static GstElement* wstFindAudioSink( GstWesterosSink *sink )
{
   GstElement *audioSink= 0;
   GstElement *pipeline= 0;
   GstElement *element, *elementPrev= 0;
   GstIterator *iterator;

   element= GST_ELEMENT_CAST(sink);
   do
   {
      if ( elementPrev )
      {
         gst_object_unref( elementPrev );
      }
      element= GST_ELEMENT_CAST(gst_element_get_parent( element ));
      if ( element )
      {
         elementPrev= pipeline;
         pipeline= element;
      }
   }
   while( element != 0 );

   if ( pipeline )
   {
      GstIterator *iterElement= gst_bin_iterate_recurse( GST_BIN(pipeline) );
      if ( iterElement )
      {
         GValue itemElement= G_VALUE_INIT;
         while( gst_iterator_next( iterElement, &itemElement ) == GST_ITERATOR_OK )
         {
            element= (GstElement*)g_value_get_object( &itemElement );
            if ( element && !GST_IS_BIN(element) )
            {
               int numSrcPads= 0;

               GstIterator *iterPad= gst_element_iterate_src_pads( element );
               if ( iterPad )
               {
                  GValue itemPad= G_VALUE_INIT;
                  while( gst_iterator_next( iterPad, &itemPad ) == GST_ITERATOR_OK )
                  {
                     GstPad *pad= (GstPad*)g_value_get_object( &itemPad );
                     if ( pad )
                     {
                        ++numSrcPads;
                     }
                     g_value_reset( &itemPad );
                  }
                  gst_iterator_free(iterPad);
               }

               if ( numSrcPads == 0 )
               {
                  GstElementClass *ec= GST_ELEMENT_GET_CLASS(element);
                  if ( ec )
                  {
                     const gchar *meta= gst_element_class_get_metadata( ec, GST_ELEMENT_METADATA_KLASS);
                     if ( meta && strstr(meta, "Sink") && strstr(meta, "Audio") )
                     {
                        audioSink= (GstElement*)gst_object_ref( element );
                        gchar *name= gst_element_get_name( element );
                        if ( name )
                        {
                           GST_DEBUG( "detected audio sink: name (%s)", name);
                           g_free( name );
                        }
                        g_value_reset( &itemElement );
                        break;
                     }
                  }
               }
            }
            g_value_reset( &itemElement );
         }
         gst_iterator_free(iterElement);
      }

      gst_object_unref(pipeline);
   }
   return audioSink;
}
#endif

static void wstSetSessionInfo( GstWesterosSink *sink )
{
   #if defined USE_AMLOGIC_MESON || defined USE_GENERIC_AVSYNC
   if ( sink->raw.conn )
   {
      GstElement *audioSink;
      GstElement *element= GST_ELEMENT(sink);
      GstClock *clock= GST_ELEMENT_CLOCK(element);
      int syncTypePrev= sink->raw.syncType;
      int sessionIdPrev= sink->raw.sessionId;
      #ifdef USE_AMLOGIC_MESON_MSYNC
      if ( sink->raw.userSession )
      {
         syncTypePrev= -1;
         sessionIdPrev= -1;
      }
      else
      {
         sink->raw.syncType= 0;
         sink->raw.sessionId= INVALID_SESSION_ID;
         audioSink= wstFindAudioSink( sink );
         if ( audioSink )
         {
            GstClock* amlclock= gst_aml_hal_asink_get_clock( audioSink );
            if (amlclock)
            {
               sink->raw.syncType= 1;
               sink->raw.sessionId= gst_aml_clock_get_session_id( amlclock );
               gst_object_unref( amlclock );
            }
            else
            {
               GST_WARNING ("no clock: vmaster mode");
            }
            gst_object_unref( audioSink );
            GST_WARNING("AmlHalAsink detected, sesison_id: %d", sink->raw.sessionId);
         }
      }
      #else
      sink->raw.syncType= 0;
      sink->raw.sessionId= 0;
      audioSink= wstFindAudioSink( sink );
      if ( audioSink )
      {
         sink->raw.syncType= 1;
         #ifdef USE_GENERIC_AVSYNC
         if ( !gst_base_sink_get_sync(GST_BASE_SINK(sink)) )
         {
            if ( sink->raw.avsctx && (sink->raw.avsctx->audioSink != audioSink) )
            {
               wstDestroyAVSyncCtx( sink, sink->raw.avsctx );
               sink->raw.avsctx= 0;
            }
            if ( !sink->raw.avsctx )
            {
               sink->raw.avsctx= wstCreateAVSyncCtx( sink );
               syncTypePrev= -1;
            }
            if ( sink->raw.avsctx )
            {
               sink->raw.avsctx->audioSink= (GstElement*)gst_object_ref(audioSink);
            }
         }
         #endif
         gst_object_unref( audioSink );
      }
      if ( clock )
      {
         const char *socClockName;
         gchar *clockName;
         clockName= gst_object_get_name(GST_OBJECT_CAST(clock));
         if ( clockName )
         {
            int sclen;
            int len= strlen(clockName);
            socClockName= getenv("WESTEROS_SINK_CLOCK");
            if ( !socClockName )
            {
               socClockName= "GstAmlSinkClock";
            }
            sclen= strlen(socClockName);
            if ( (len == sclen) && !strncmp(clockName, socClockName, len) )
            {
               sink->raw.syncType= 1;
               /* TBD: set sessionid */
            }
            g_free( clockName );
         }
      }
      if ( sink->resAssignedId >= 0 )
      {
         sink->raw.sessionId= sink->resAssignedId;
      }
      #endif
      if ( (syncTypePrev != sink->raw.syncType) || (sessionIdPrev != sink->raw.sessionId) )
      {
         wstSendSessionInfoVideoClientConnection( sink->raw.conn );
      }
   }
   #endif
}

static void wstSendFlushVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[4];
      int len;
      int sentLen;

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 1;
      mbody[len++]= 'S';

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent flush to video server");
         FRAME("sent flush to video server");
      }
      UNLOCK_CONN( conn );
   }
}

static void wstSendPauseVideoClientConnection( WstRawVideoClientConnection *conn, bool pause )
{
   if ( conn )
   {
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[13];
      int len;
      int sentLen;

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 10;
      mbody[len++]= 'P';
      mbody[len++]= (pause ? 1 : 0);
      len += putU32( &mbody[len], conn->sink->segment.rate*10000LL );
      len += putU32( &mbody[len], 10000LL );

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent pause %d (rate %f) to video server", pause, conn->sink->segment.rate);
         FRAME("sent pause %d (rate %f) to video server", pause, conn->sink->segment.rate);
      }
      UNLOCK_CONN( conn );
   }
}

static void wstSendRectVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[20];
      int len;
      int sentLen;
      int vx, vy, vw, vh;
      GstWesterosSink *sink= conn->sink;

      vx= sink->raw.videoX;
      vy= sink->raw.videoY;
      vw= sink->raw.videoWidth;
      vh= sink->raw.videoHeight;
      if ( needBounds(sink) )
      {
         wstGetVideoBounds( sink, &vx, &vy, &vw, &vh );
      }

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 17;
      mbody[len++]= 'W';
      len += putU32( &mbody[len], vx );
      len += putU32( &mbody[len], vy );
      len += putU32( &mbody[len], vw );
      len += putU32( &mbody[len], vh );

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent position to video server");
         FRAME("sent position to video server");
      }
      UNLOCK_CONN( conn );
   }
}

static void wstSendRateVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      struct msghdr msg;
      struct iovec iov[1];
      unsigned char mbody[12];
      int len;
      int sentLen;
      GstWesterosSink *sink= conn->sink;

      LOCK_CONN( conn );
      msg.msg_name= NULL;
      msg.msg_namelen= 0;
      msg.msg_iov= iov;
      msg.msg_iovlen= 1;
      msg.msg_control= 0;
      msg.msg_controllen= 0;
      msg.msg_flags= 0;

      len= 0;
      mbody[len++]= 'V';
      mbody[len++]= 'S';
      mbody[len++]= 9;
      mbody[len++]= 'R';
      len += putU32( &mbody[len], sink->raw.frameRateFractionNum );
      len += putU32( &mbody[len], sink->raw.frameRateFractionDenom );

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));

      if ( sentLen == len )
      {
         GST_LOG("sent frame rate to video server");
         FRAME("sent frame rate to video server");
      }
      UNLOCK_CONN( conn );
   }
}

static void wstProcessMessagesVideoClientConnection( WstRawVideoClientConnection *conn )
{
   if ( conn )
   {
      GstWesterosSink *sink= conn->sink;
      struct pollfd pfd;
      int rc;

      pfd.fd= conn->socketFd;
      pfd.events= POLLIN;
      pfd.revents= 0;

      rc= poll( &pfd, 1, 0);
      if ( rc == 1 )
      {
         struct msghdr msg;
         struct iovec iov[1];
         unsigned char mbody[256];
         unsigned char *m= mbody;
         int len;

         iov[0].iov_base= (char*)mbody;
         iov[0].iov_len= sizeof(mbody);

         msg.msg_name= NULL;
         msg.msg_namelen= 0;
         msg.msg_iov= iov;
         msg.msg_iovlen= 1;
         msg.msg_control= 0;
         msg.msg_controllen= 0;
         msg.msg_flags= 0;

         do
         {
            len= recvmsg( conn->socketFd, &msg, 0 );
         }
         while ( (len < 0) && (errno == EINTR));

         while ( len >= 4 )
         {
            if ( (m[0] == 'V') && (m[1] == 'S') )
            {
               int mlen, id;
               mlen= m[2];
               if ( len >= (mlen+3) )
               {
                  id= m[3];
                  switch( id )
                  {
                     case 'R':
                        if ( mlen >= 5)
                        {
                          int rate= getU32( &m[4] );
                          GST_DEBUG("got rate %d from video server", rate);
                          conn->serverRefreshRate= rate;
                          if ( rate )
                          {
                             conn->serverRefreshPeriod= 1000000LL/rate;
                          }
                          FRAME("got rate %d (period %lld us) from video server", rate, conn->serverRefreshPeriod);
                        }
                        break;
                     case 'B':
                        if ( mlen >= 5)
                        {
                          int bi= getU32( &m[4] );
                          if ( sink->raw.drmBuffer[bi].locked )
                          {
                             FRAME("out:       release received for buffer %d (%d)", bi, bi);
                             if ( drmUnlockBuffer( sink, bi ) )
                             {
                                drmReleaseBuffer( sink, bi );
                             }
                          }
                          else
                          {
                             GST_ERROR("release received for non-locked buffer %d (%d)", bi, bi );
                             FRAME("out:       error: release received for non-locked buffer %d (%d)", bi, bi);
                          }
                        }
                        break;
                     case 'S':
                        if ( mlen >= 13)
                        {
                           /* set position from frame currently presented by the video server */
                           guint64 frameTime= getS64( &m[4] );
                           sink->raw.numDropped= getU32( &m[12] );
                           FRAME( "out:       status received: frameTime %lld numDropped %d", frameTime, sink->raw.numDropped);
                           if ( (frameTime != -1LL) && (sink->prevPositionSegmentStart != 0xFFFFFFFFFFFFFFFFLL) )
                           {
                              gint64 currentNano= frameTime*1000LL;
                              gint64 firstNano= ((sink->firstPTS/90LL)*GST_MSECOND)+((sink->firstPTS%90LL)*GST_MSECOND/90LL);
                              sink->position= sink->positionSegmentStart + currentNano - firstNano;
                              sink->currentPTS= currentNano / (GST_SECOND/90000LL);
                              GST_LOG("receive frameTime: %lld position %lld", currentNano, sink->position);
                              if (sink->raw.frameDisplayCount == 0)
                              {
                                  sink->raw.emitFirstFrameSignal= TRUE;
                              }
                              ++sink->raw.frameDisplayCount;
                              if ( sink->timeCodePresent && sink->enableTimeCodeSignal )
                              {
                                 sink->timeCodePresent( sink, sink->position, g_signals[SIGNAL_TIMECODE] );
                              }
                           }
                        }
                        break;
                     case 'U':
                        if ( mlen >= 9 )
                        {
                           guint64 frameTime= getS64( &m[4] );
                           GST_INFO( "underflow received: frameTime %lld eosEventSeen %d", frameTime, sink->eosEventSeen);
                           FRAME( "out:       underflow received: frameTime %lld", frameTime);
                           if ( !sink->eosEventSeen )
                           {
                              sink->raw.emitUnderflowSignal= TRUE;
                           }
                        }
                        break;
                     case 'Z':
                        if ( mlen >= 13)
                        {
                          int globalZoomActive= getU32( &m[4] );
                          int allow4kZoom= getU32( &m[8] );
                          int zoomMode= getU32( &m[12] );
                          GST_DEBUG("got zoom-mode %d from video server (globalZoomActive %d allow4kZoom %d)", zoomMode);
                          if ( sink->raw.zoomModeUser == -1 )
                          {
                             sink->raw.zoomModeGlobal= globalZoomActive;
                             if ( !globalZoomActive )
                             {
                                sink->raw.zoomMode= ZOOM_NONE;
                             }
                          }
                          sink->raw.allow4kZoom= allow4kZoom;
                          if ( sink->raw.zoomModeGlobal == TRUE )
                          {
                             if ( (zoomMode >= ZOOM_NONE) && (zoomMode <= ZOOM_ZOOM) )
                             {
                                sink->raw.zoomMode= zoomMode;
                                sink->raw.pixelAspectRatioChanged= TRUE;
                             }
                          }
                          else
                          {
                             GST_DEBUG("global zoom disabled: ignore server value");
                          }
                        }
                        break;
                     case 'D':
                        if ( mlen >= 5)
                        {
                          int debugLevel= getU32( &m[4] );
                          GST_DEBUG("got video-debug-level %d from video server", debugLevel);
                          if ( (debugLevel >= 0) && (debugLevel <= 7) )
                          {
                             if ( debugLevel == 0 )
                             {
                                gst_debug_category_reset_threshold( gst_westeros_sink_debug );
                             }
                             else
                             {
                                gst_debug_category_set_threshold( gst_westeros_sink_debug, (GstDebugLevel)debugLevel );
                             }
                          }
                        }
                        break;
                     default:
                        break;
                  }
                  m += (mlen+3);
                  len -= (mlen+3);
               }
               else
               {
                  len= 0;
               }
            }
            else
            {
               len= 0;
            }
         }
         if ( sink->raw.emitFirstFrameSignal )
         {
            sink->raw.emitFirstFrameSignal= FALSE;
            LOCK(sink);
            sink->raw.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
            UNLOCK(sink);
         }
         if ( sink->raw.emitUnderflowSignal )
         {
            sink->raw.emitUnderflowSignal= FALSE;
            LOCK(sink);
            sink->raw.underflowThread= g_thread_new("westerossinkUF", wstUnderflowThread, sink);
            UNLOCK(sink);
         }
      }
   }
}

static bool wstSendFrameVideoClientConnection( WstRawVideoClientConnection *conn, int buffIndex )
{
   bool result= false;
   GstWesterosSink *sink= conn->sink;
   int sentLen;

   if ( conn  )
   {
      struct msghdr msg;
      struct cmsghdr *cmsg;
      struct iovec iov[1];
      unsigned char mbody[4+64];
      char cmbody[CMSG_SPACE(3*sizeof(int))];
      int i, len;
      int *fd;
      int numFdToSend;
      int frameFd0= -1, frameFd1= -1, frameFd2= -1;
      int fdToSend0= -1, fdToSend1= -1, fdToSend2= -1;
      int offset0, offset1, offset2;
      int stride0, stride1, stride2;
      uint32_t pixelFormat;
      int bufferId= -1;
      int vx, vy, vw, vh;

      wstProcessMessagesVideoClientConnection( conn );

      if ( buffIndex >= 0 )
      {
         sink->raw.resubFd= -1;

         bufferId= sink->raw.drmBuffer[buffIndex].bufferId;

         numFdToSend= 1;
         offset0= offset1= offset2= 0;
         stride0= stride1= stride2= sink->raw.frameWidth;
         frameFd0= sink->raw.drmBuffer[buffIndex].fd[0];
         stride0= sink->raw.drmBuffer[buffIndex].pitch[0];
         frameFd1= sink->raw.drmBuffer[buffIndex].fd[1];
         stride1= sink->raw.drmBuffer[buffIndex].pitch[1];
         if ( frameFd1 < 0 )
         {
            offset1= sink->raw.frameWidth*sink->raw.frameHeight;
            stride1= stride0;
         }

         pixelFormat= sink->raw.frameFormatOut;

         fdToSend0= fcntl( frameFd0, F_DUPFD_CLOEXEC, 0 );
         if ( fdToSend0 < 0 )
         {
            GST_ERROR("wstSendFrameVideoClientConnection: failed to dup fd0");
            goto exit;
         }
         if ( frameFd1 >= 0 )
         {
            fdToSend1= fcntl( frameFd1, F_DUPFD_CLOEXEC, 0 );
            if ( fdToSend1 < 0 )
            {
               GST_ERROR("wstSendFrameVideoClientConnection: failed to dup fd1");
               goto exit;
            }
            ++numFdToSend;
         }
         if ( frameFd2 >= 0 )
         {
            fdToSend2= fcntl( frameFd2, F_DUPFD_CLOEXEC, 0 );
            if ( fdToSend2 < 0 )
            {
               GST_ERROR("wstSendFrameVideoClientConnection: failed to dup fd2");
               goto exit;
            }
            ++numFdToSend;
         }

         vx= sink->raw.videoX;
         vy= sink->raw.videoY;
         vw= sink->raw.videoWidth;
         vh= sink->raw.videoHeight;
         if ( needBounds(sink) )
         {
            wstGetVideoBounds( sink, &vx, &vy, &vw, &vh );
         }

         LOCK_CONN( conn );
         i= 0;
         mbody[i++]= 'V';
         mbody[i++]= 'S';
         mbody[i++]= 65;
         mbody[i++]= 'F';
         i += putU32( &mbody[i], conn->sink->raw.frameWidth );
         i += putU32( &mbody[i], conn->sink->raw.frameHeight );
         i += putU32( &mbody[i], pixelFormat );
         i += putU32( &mbody[i], vx );
         i += putU32( &mbody[i], vy );
         i += putU32( &mbody[i], vw );
         i += putU32( &mbody[i], vh );
         i += putU32( &mbody[i], offset0 );
         i += putU32( &mbody[i], stride0 );
         i += putU32( &mbody[i], offset1 );
         i += putU32( &mbody[i], stride1 );
         i += putU32( &mbody[i], offset2 );
         i += putU32( &mbody[i], stride2 );
         i += putU32( &mbody[i], bufferId );
         i += putS64( &mbody[i], sink->raw.drmBuffer[buffIndex].frameTime );

         iov[0].iov_base= (char*)mbody;
         iov[0].iov_len= i;

         cmsg= (struct cmsghdr*)cmbody;
         cmsg->cmsg_len= CMSG_LEN(numFdToSend*sizeof(int));
         cmsg->cmsg_level= SOL_SOCKET;
         cmsg->cmsg_type= SCM_RIGHTS;

         msg.msg_name= NULL;
         msg.msg_namelen= 0;
         msg.msg_iov= iov;
         msg.msg_iovlen= 1;
         msg.msg_control= cmsg;
         msg.msg_controllen= cmsg->cmsg_len;
         msg.msg_flags= 0;

         fd= (int*)CMSG_DATA(cmsg);
         fd[0]= fdToSend0;
         if ( fdToSend1 >= 0 )
         {
            fd[1]= fdToSend1;
         }
         if ( fdToSend2 >= 0 )
         {
            fd[2]= fdToSend2;
         }
         GST_LOG( "%lld: send frame: %d, fd (%d, %d, %d [%d, %d, %d])", getCurrentTimeMillis(), buffIndex, frameFd0, frameFd1, frameFd2, fdToSend0, fdToSend1, fdToSend2);
         drmLockBuffer( sink, buffIndex );
         FRAME("out:       send frame %d buffer %d (%d)", conn->sink->raw.frameOutCount, conn->sink->raw.drmBuffer[buffIndex].bufferId, buffIndex);

         do
         {
            sentLen= sendmsg( conn->socketFd, &msg, 0 );
         }
         while ( (sentLen < 0) && (errno == EINTR));

         conn->sink->raw.drmBuffer[buffIndex].frameNumber= conn->sink->raw.frameOutCount;

         if ( sentLen == iov[0].iov_len )
         {
            result= true;
         }
         else
         {
            FRAME("out:       failed send frame %d buffer %d (%d)", conn->sink->raw.frameOutCount, conn->sink->raw.drmBuffer[buffIndex].bufferId, buffIndex);
            if ( drmUnlockBuffer( sink, buffIndex ) )
            {
               drmReleaseBuffer( sink, buffIndex );
            }
         }
         UNLOCK_CONN( conn );
      }

exit:
      if ( fdToSend0 >= 0 )
      {
         close( fdToSend0 );
      }
      if ( fdToSend1 >= 0 )
      {
         close( fdToSend1 );
      }
      if ( fdToSend2 >= 0 )
      {
         close( fdToSend2 );
      }
   }
   return result;
}

static gpointer wstDispatchThread(gpointer data)
{
   GstWesterosSink *sink= (GstWesterosSink*)data;
   if ( sink->display )
   {
      GST_DEBUG("dispatchThread: enter");
      while( !sink->raw.quitDispatchThread )
      {
         if ( wl_display_dispatch_queue( sink->display, sink->queue ) == -1 )
         {
            break;
         }
      }
      GST_DEBUG("dispatchThread: exit");
   }
   return NULL;
}

static gpointer wstEOSDetectionThread(gpointer data)
{
   GstWesterosSink *sink= (GstWesterosSink*)data;
   int outputFrameCount, count, eosCountDown;
   int displayCount;
   bool videoPlaying;
   bool eosEventSeen;
   double frameRate;

   GST_DEBUG("wstVideoEOSThread: enter");

   eosCountDown= 10;
   LOCK(sink)
   outputFrameCount= sink->raw.frameOutCount;
   frameRate= (sink->raw.frameRate > 0.0 ? sink->raw.frameRate : 30.0);
   UNLOCK(sink);
   while( !sink->raw.quitEOSDetectionThread )
   {
      usleep( 1000000/frameRate );

      if ( !sink->raw.quitEOSDetectionThread )
      {
         LOCK(sink)
         count= sink->raw.frameOutCount;
         displayCount= sink->raw.frameDisplayCount + sink->raw.numDropped;
         videoPlaying= sink->raw.videoPlaying;
         eosEventSeen= sink->eosEventSeen;
         #ifdef USE_GENERIC_AVSYNC
         wstUpdateAVSyncCtx( sink, sink->raw.avsctx );
         #endif
         UNLOCK(sink)

         if ( sink->windowChange )
         {
            sink->windowChange= false;
            gst_westeros_sink_raw_update_video_position( sink );
         }

         if ( eosEventSeen )
         {
            GST_DEBUG("waiting for eos: frameOutCount %d displayCount %d (%d+%d)\n", count, displayCount, sink->raw.frameDisplayCount, sink->raw.numDropped);
            wstProcessMessagesVideoClientConnection( sink->raw.conn );
         }
         if ( videoPlaying && eosEventSeen && (count == displayCount) && (outputFrameCount == count) )
         {
            --eosCountDown;
            if ( eosCountDown == 0 )
            {
               g_print("westeros-sink: EOS detected\n");
               gst_element_post_message (GST_ELEMENT_CAST(sink), gst_message_new_eos(GST_OBJECT_CAST(sink)));
               break;
            }
         }
         else
         {
            outputFrameCount= count;
            eosCountDown= 10;
         }
      }
   }

   if ( !sink->raw.quitEOSDetectionThread )
   {
      GThread *thread= sink->raw.eosDetectionThread;
      g_thread_unref( sink->raw.eosDetectionThread );
      sink->raw.eosDetectionThread= NULL;
   }

   GST_DEBUG("wstVideoEOSThread: exit");

   return NULL;
}

static gpointer wstFirstFrameThread(gpointer data)
{
   GstWesterosSink *sink= (GstWesterosSink*)data;

   if ( sink )
   {
      GST_DEBUG("wstFirstFrameThread: emit first frame signal");
      g_signal_emit (G_OBJECT (sink), g_signals[SIGNAL_FIRSTFRAME], 0, 2, NULL);
      LOCK(sink);
      g_thread_unref( sink->raw.firstFrameThread );
      sink->raw.firstFrameThread= NULL;
      UNLOCK(sink);
   }

   return NULL;
}

static gpointer wstUnderflowThread(gpointer data)
{
   GstWesterosSink *sink= (GstWesterosSink*)data;

   if ( sink )
   {
      GST_DEBUG("wstUnderflowThread: emit underflow signal");
      g_signal_emit (G_OBJECT (sink), g_signals[SIGNAL_UNDERFLOW], 0, 0, NULL);
      LOCK(sink);
      g_thread_unref( sink->raw.underflowThread );
      sink->raw.underflowThread= NULL;
      UNLOCK(sink);
   }

   return NULL;
}

static void wstBuildSinkCaps_raw( GstWesterosSinkClass *klass )
{
   GstCaps *caps= 0;
   GstCaps *capsTemp= 0;
   GstPadTemplate *padTemplate= 0;

   caps= gst_caps_new_empty();
   if ( caps )
   {
      capsTemp= gst_caps_from_string(
                                       "video/x-raw, " \
                                       "format=(string) { NV12, I420, YU12 }"
                                    );
      if ( capsTemp )
      {
         gst_caps_append( caps, capsTemp );
         capsTemp =0;
      }

      capsTemp= gst_caps_from_string(
                                       "video/x-westeros-raw "
                                    );
      if ( capsTemp )
      {
         gst_caps_append( caps, capsTemp );
         capsTemp =0;
      }

      padTemplate= gst_pad_template_new( "sink",
                                         GST_PAD_SINK,
                                         GST_PAD_ALWAYS,
                                         caps );
      if ( padTemplate )
      {
         GstElementClass *gstelement_class= (GstElementClass *)klass;
         gst_element_class_add_pad_template(gstelement_class, padTemplate);
         padTemplate= 0;
      }
      else
      {
         GST_ERROR("wstBuildSinkCaps_raw: gst_pad_template_new failed");
      }

      gst_caps_unref( caps );
   }
   else
   {
      GST_ERROR("wstBuildSinkCaps_raw: gst_caps_new_empty failed");
   }
}

#define DEFAULT_DRM_NAME "/dev/dri/card0"

static bool drmInit( GstWesterosSink *sink )
{
   bool result= false;
   const char *drmName;

   drmName= getenv("WESTEROS_SINK_DRM_NAME");
   if ( !drmName )
   {
      drmName= DEFAULT_DRM_NAME;
   }

   GST_DEBUG("drmInit");
   sink->raw.drmFd= open( drmName, O_RDWR | O_CLOEXEC );
   if ( sink->raw.drmFd < 0 )
   {
      GST_ERROR("Failed to open drm node (%s): %d", drmName, errno);
      goto exit;
   }

   result= true;

exit:
   return result;
}

static void drmTerm( GstWesterosSink *sink )
{
   int i;
   GST_DEBUG("drmTerm");
   if ( sink->raw.eosDetectionThread )
   {
      sink->raw.quitEOSDetectionThread= TRUE;
      g_thread_join( sink->raw.eosDetectionThread );
      sink->raw.eosDetectionThread= NULL;
   }
   if ( sink->raw.dispatchThread )
   {
      sink->raw.quitDispatchThread= TRUE;
      g_thread_join( sink->raw.dispatchThread );
      sink->raw.dispatchThread= NULL;
   }
   for( i= 0; i < WST_NUM_DRM_BUFFERS; ++i )
   {
      drmFreeBuffer( sink, i );
   }
   if ( sink->raw.drmFd >= 0 )
   {
      close( sink->raw.drmFd );
      sink->raw.drmFd= -1;
   }
}

static bool drmAllocBuffer( GstWesterosSink *sink, int buffIndex, int width, int height )
{
   bool result= false;
   WstDrmBuffer *drmBuff= 0;
   if ( buffIndex < WST_NUM_DRM_BUFFERS )
   {
      struct drm_mode_create_dumb createDumb;
      struct drm_mode_map_dumb mapDumb;
      int i, rc;

      drmBuff= &sink->raw.drmBuffer[buffIndex];

      drmBuff->width= width;
      drmBuff->height= height;
      GST_LOG("drmAllocBuffer: (%dx%d)", width, height);

      width= ((width+63)&~63);

      memset( &createDumb, 0, sizeof(createDumb) );
      createDumb.width= width;
      createDumb.height= height;
      #ifdef USE_SINGLE_BUFFER_NV12
      createDumb.height += height/2;
      #endif
      createDumb.bpp= 8;
      rc= ioctl( sink->raw.drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_CREATE_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      memset( &mapDumb, 0, sizeof(mapDumb) );
      mapDumb.handle= createDumb.handle;
      rc= ioctl( sink->raw.drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_MAP_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      drmBuff->handle[0]= createDumb.handle;
      drmBuff->pitch[0]= createDumb.pitch;
      drmBuff->size[0]= createDumb.size;
      drmBuff->offset[0]= mapDumb.offset;

      rc= drmPrimeHandleToFD( sink->raw.drmFd, drmBuff->handle[0], DRM_CLOEXEC | DRM_RDWR, &drmBuff->fd[0] );
      if ( rc )
      {
         GST_ERROR("drmPrimeHandleToFD failed: rc %d errno %d", rc, errno);
         goto exit;
      }

      #ifdef USE_SINGLE_BUFFER_NV12
      drmBuff->fd[1]= -1;
      #else
      memset( &createDumb, 0, sizeof(createDumb) );
      createDumb.width= width;
      createDumb.height= height/2;
      createDumb.bpp= 8;
      rc= ioctl( sink->raw.drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_CREATE_DUMB failed: rc %d errno %d\n", rc, errno);
         goto exit;
      }
      memset( &mapDumb, 0, sizeof(mapDumb) );
      mapDumb.handle= createDumb.handle;
      rc= ioctl( sink->raw.drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_MAP_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      drmBuff->handle[1]= createDumb.handle;
      drmBuff->pitch[1]= createDumb.pitch;
      drmBuff->size[1]= createDumb.size;
      drmBuff->offset[1]= mapDumb.offset;

      rc= drmPrimeHandleToFD( sink->raw.drmFd, drmBuff->handle[1], DRM_CLOEXEC | DRM_RDWR, &drmBuff->fd[1] );
      if ( rc )
      {
         GST_ERROR("drmPrimeHandleToFD failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      #endif

      drmBuff->bufferId= buffIndex;
      drmBuff->localAlloc= true;

      result= true;
   }
exit:
   if ( !result )
   {
      drmFreeBuffer( sink, buffIndex );
   }
   return result;
}

static void drmFreeBuffer( GstWesterosSink *sink, int buffIndex )
{
   int i;
   WstDrmBuffer *drmBuff= 0;
   if (
        (buffIndex < WST_NUM_DRM_BUFFERS) &&
        (sink->raw.drmBuffer[buffIndex].width != -1) &&
        (sink->raw.drmBuffer[buffIndex].height != -1)
      )
   {
      GST_LOG("drmFreeBuffer: (%dx%d)", sink->raw.drmBuffer[buffIndex].width, sink->raw.drmBuffer[buffIndex].height);
   }
   drmBuff= &sink->raw.drmBuffer[buffIndex];
   if ( drmBuff->localAlloc )
   {
      for( i= 0; i < WST_MAX_PLANE; ++i )
      {
         int *fd, *handle;
         fd= &drmBuff->fd[i];
         handle= &drmBuff->handle[i];
         if ( *fd >= 0 )
         {
            close( *fd );
            *fd= -1;
         }
         if ( *handle )
         {
            struct drm_mode_destroy_dumb destroyDumb;
            destroyDumb.handle= *handle;
            ioctl( sink->raw.drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyDumb );
            *handle= 0;
         }
      }
   }
   else if ( drmBuff->gstbuf )
   {
      int fd= drmBuff->fd[0];

      gst_buffer_unref( drmBuff->gstbuf );
      drmBuff->gstbuf= 0;
      drmBuff->fd[0]= -1;
      drmBuff->fd[1]= -1;

      if ( sink->raw.expectDummyBuffers )
      {
         GstPad *pad= GST_BASE_SINK(sink)->sinkpad;
         if ( pad )
         {
            GstStructure *structure;
            structure= gst_structure_new("westeros-raw-release",
                                         "fd", G_TYPE_INT, fd,
                                          NULL );
            if ( structure )
            {
               GST_DEBUG("push westeros-raw-release: fd %d", fd);
               gst_pad_push_event( pad, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, structure));
            }
         }
      }
   }
}

static void drmLockBuffer( GstWesterosSink *sink, int buffIndex )
{
   sink->raw.drmBuffer[buffIndex].locked= true;
   ++sink->raw.drmBuffer[buffIndex].lockCount;
}

static bool drmUnlockBuffer( GstWesterosSink *sink, int buffIndex )
{
   bool unlocked= false;
   if ( !sink->raw.drmBuffer[buffIndex].locked )
   {
      GST_ERROR("attempt to unlock buffer that is not locked: index %d", buffIndex);
   }
   if ( sink->raw.drmBuffer[buffIndex].lockCount > 0 )
   {
      if ( --sink->raw.drmBuffer[buffIndex].lockCount == 0 )
      {
         sink->raw.drmBuffer[buffIndex].locked= false;
         unlocked= true;
      }
   }
   return unlocked;
}

static void drmUnlockAllBuffers( GstWesterosSink *sink )
{
   WstDrmBuffer *drmBuff= 0;
   int buffIndex;
   bool didUnlock= false;
   for( buffIndex= 0; buffIndex < WST_NUM_DRM_BUFFERS; ++buffIndex )
   {
      drmBuff= &sink->raw.drmBuffer[buffIndex];
      if ( drmBuff->locked )
      {
         drmBuff->locked= false;
         drmBuff->lockCount= 0;
         didUnlock= true;
      }
   }
   if ( didUnlock )
   {
      sem_post( &sink->raw.drmBuffSem );
   }
}

#ifdef USE_GST_ALLOCATORS
static WstDrmBuffer *drmImportBuffer( GstWesterosSink *sink, GstBuffer *buffer )
{
   WstDrmBuffer *drmBuff= 0;
   int buffIndex;
   int rc;

   for ( ; ; )
   {
      rc= sem_trywait( &sink->raw.drmBuffSem );
      if ( rc )
      {
         if ( errno == EAGAIN )
         {
            usleep( 1000 );
            wstProcessMessagesVideoClientConnection( sink->raw.conn );
            continue;
         }
      }
      break;
   }

   for( buffIndex= 0; buffIndex < WST_NUM_DRM_BUFFERS; ++buffIndex )
   {
      drmBuff= &sink->raw.drmBuffer[buffIndex];
      if ( !drmBuff->locked )
      {
         int i, imax;
         GstMemory *mem;
         #ifdef USE_GST_VIDEO
         GstVideoMeta *meta= gst_buffer_get_video_meta(buffer);
         #endif
         drmBuff->width= sink->raw.frameWidth;
         drmBuff->height= sink->raw.frameHeight;
         imax= gst_buffer_n_memory( buffer );
         if ( imax > WST_MAX_PLANE ) imax= WST_MAX_PLANE;
         for( i= 0; i < imax; ++i )
         {
            mem= gst_buffer_peek_memory( buffer, i );
            if ( mem )
            {
               gsize offset;
               drmBuff->fd[i]= gst_dmabuf_memory_get_fd( mem );
               drmBuff->size[i]= gst_memory_get_sizes( mem, &offset, NULL );
               drmBuff->offset[i]= offset;
               switch( sink->raw.frameFormatStream )
               {
                  case DRM_FORMAT_NV12:
                  case DRM_FORMAT_NV21:
                     sink->raw.frameFormatOut= sink->raw.frameFormatStream;
                     #ifdef USE_GST_VIDEO
                     if ( meta )
                     {
                        drmBuff->pitch[i]= meta->stride[i];
                     }
                     else
                     #endif
                     {
                        drmBuff->pitch[i]= ((sink->raw.frameWidth+63) & ~63);
                     }
                     break;
                  default:
                     GST_ERROR("Unsupported format (0x%x) for dma-buf import", sink->raw.frameFormatStream );
                     break;
               }
               GST_LOG("drmImportBuffer: buffer %p fmt %X fd %d size %d pitch %d offset %llu", buffer, sink->raw.frameFormatStream,
                       drmBuff->fd[i], drmBuff->size[i], drmBuff->pitch[i], drmBuff->offset[i]);
            }
            else
            {
               drmBuff->fd[i]= -1;
               drmBuff->size[i]= 0;
               drmBuff->offset[i]= 0;
               drmBuff->pitch[i]= 0;
            }
         }
         drmBuff->bufferId= buffIndex;
         drmBuff->localAlloc= false;
         drmBuff->gstbuf= gst_buffer_ref(buffer);
         break;
      }
      else
      {
         drmBuff= 0;
      }
   }
   return drmBuff;
}
#endif

static WstDrmBuffer *drmGetBuffer( GstWesterosSink *sink, int width, int height )
{
   WstDrmBuffer *drmBuff= 0;
   int buffIndex;
   int rc;

   GST_BASE_SINK_PREROLL_UNLOCK(GST_BASE_SINK(sink));
   for ( ; ; )
   {
      rc= sem_trywait( &sink->raw.drmBuffSem );
      if ( rc )
      {
         if ( sink->flushStarted )
         {
            break;
         }
         if ( errno == EAGAIN )
         {
            usleep( 1000 );
            wstProcessMessagesVideoClientConnection( sink->raw.conn );
            continue;
         }
      }
      break;
   }
   GST_BASE_SINK_PREROLL_LOCK(GST_BASE_SINK(sink));

   if ( sink->flushStarted )
   {
      if ( !rc )
      {
         /* If we succeeded in decrementing semaphore count
          * above but are not returning the buffer, do a post */
         sem_post( &sink->raw.drmBuffSem );
      }
      goto exit;
   }

   for( buffIndex= 0; buffIndex < WST_NUM_DRM_BUFFERS; ++buffIndex )
   {
      drmBuff= &sink->raw.drmBuffer[buffIndex];
      if ( !drmBuff->locked )
      {
         if ( (drmBuff->width != width) || (drmBuff->height != height) )
         {
            drmFreeBuffer( sink, buffIndex );
            if ( !drmAllocBuffer( sink, buffIndex, width, height ) )
            {
               drmBuff= 0;
            }
         }
         break;
      }
      else
      {
         drmBuff= 0;
      }
   }
exit:
   return drmBuff;
}

static void drmReleaseBuffer( GstWesterosSink *sink, int buffIndex )
{
   if ( !sink->raw.drmBuffer[buffIndex].locked )
   {
      int rc;
      sink->raw.drmBuffer[buffIndex].frameNumber= -1;
      FRAME("out:       release buffer %d (%d)", sink->raw.drmBuffer[buffIndex].bufferId, buffIndex);
      GST_LOG( "%lld: release: buffer %d (%d)", getCurrentTimeMillis(), sink->raw.drmBuffer[buffIndex].bufferId, buffIndex);
      if ( !sink->raw.drmBuffer[buffIndex].localAlloc )
      {
         drmFreeBuffer( sink, buffIndex );
      }
      sem_post( &sink->raw.drmBuffSem );
   }
}

static int sinkAcquireVideo( GstWesterosSink *sink )
{
   int result= 0;

   GST_DEBUG("sinkAcquireVideo: enter");

   LOCK(sink);
   sink->raw.haveHardware= TRUE;
   UNLOCK(sink);

   result= 1;
   GST_DEBUG("sinkAcquireVideo: exit: %d", result);

   return result;
}

static void sinkReleaseVideo( GstWesterosSink *sink )
{
   GST_DEBUG("sinkReleaseVideo: enter");

   LOCK(sink);
   sink->raw.haveHardware= FALSE;
   UNLOCK(sink);

   wstSinkRawStopVideo( sink );

   GST_DEBUG("sinkReleaseVideo: exit");
}

static int sinkAcquireResources( GstWesterosSink *sink )
{
   int result= 0;

   result= sinkAcquireVideo( sink );

   return result;
}

static void sinkReleaseResources( GstWesterosSink *sink )
{
   sinkReleaseVideo( sink );
}

static GstStructure *wstSinkGetStats( GstWesterosSink * sink )
{
   g_return_val_if_fail (sink != NULL, NULL);
   return gst_structure_new ("application/x-gst-base-sink-stats",
      "dropped", G_TYPE_UINT64, (guint64)sink->raw.numDropped,
      "rendered", G_TYPE_UINT64, (guint64)sink->raw.frameDisplayCount, NULL);
}

#ifdef USE_GENERIC_AVSYNC
#define AVSYNC_PREFIX "westeros-sink-av-"
#define AVSYNC_TEMPLATE "/tmp/" AVSYNC_PREFIX "%d-"
static void wstPruneAVSyncFiles( GstWesterosSink *sink )
{
   DIR *dir;
   struct dirent *result;
   struct stat fileinfo;
   int prefixLen;
   int pid, rc;
   const char *path;
   char work[34];
   path= getenv("XDG_RUNTIME_DIR");
   if ( path )
   {
      if ( NULL != (dir = opendir( path )) )
      {
         prefixLen= strlen(AVSYNC_PREFIX);
         while( NULL != (result = readdir( dir )) )
         {
            if ( (result->d_type != DT_DIR) &&
                !strncmp(result->d_name, AVSYNC_PREFIX, prefixLen) )
            {
               snprintf( work, sizeof(work), "%s/%s", path, result->d_name);
               if ( sscanf( work, AVSYNC_TEMPLATE, &pid ) == 1 )
               {
                  // Check if the pid of this temp file is still valid
                  snprintf(work, sizeof(work), "/proc/%d", pid);
                  rc= stat( work, &fileinfo );
                  if ( rc )
                  {
                     // The pid is not valid, delete the file
                     snprintf( work, sizeof(work), "%s/%s", path, result->d_name);
                     GST_DEBUG("removing temp file: %s", work);
                     remove( work );
                  }
               }
            }
         }

         closedir( dir );
      }
   }
}

static AVSyncCtx* wstCreateAVSyncCtx( GstWesterosSink *sink )
{
   AVSyncCtx *avsctx= 0;
   int static count= 0;
   int pid, len, rc;
   pthread_mutexattr_t attr;
   const char *path;
   char name[PATH_MAX];
   AVSyncCtrl avsctrl;

   pid= getpid();

   path= getenv("XDG_RUNTIME_DIR");
   if ( !path )
   {
      GST_ERROR("XDG_RUNTIME_DIR is not set");
      goto exit;
   }

   len= snprintf( name, PATH_MAX, "%s/%s%d-%d", path, AVSYNC_PREFIX, pid, count ) + 1;
   if ( len < 0 )
   {
      GST_ERROR("error building avs control file name");
      goto exit;
   }

   if ( len > PATH_MAX )
   {
      GST_ERROR("avs control file name length exceeds max length %d", PATH_MAX );
      goto exit;
   }

   rc= pthread_mutexattr_init( &attr );
   if ( rc )
   {
      GST_ERROR("pthread_mutexattr_init failed: %d", rc);
      goto exit;
   }

   rc= pthread_mutexattr_setpshared( &attr, PTHREAD_PROCESS_SHARED );
   if ( rc )
   {
      GST_ERROR("pthread_mutexattr_setpshared failed: %d", rc);
      goto exit;
   }

   avsctx= (AVSyncCtx*)calloc( 1, sizeof(AVSyncCtx) );
   if ( avsctx )
   {
      avsctx->ctrlSize= sizeof(AVSyncCtrl);
      strncpy( avsctx->name, name, PATH_MAX);

      avsctx->fd= open( name,
                       (O_CREAT|O_CLOEXEC|O_RDWR),
                       (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) );
      if ( avsctx->fd < 0 )
      {
         GST_ERROR("Error creating avs control file (%s) errno %d", name, errno);
         goto error_exit;
      }

      memset( &avsctrl, 0, avsctx->ctrlSize );
      rc= write( avsctx->fd, &avsctrl, avsctx->ctrlSize );
      if ( rc < 0 )
      {
         GST_ERROR("Error writing avs control file: errno %d", errno);
         goto error_exit;
      }

      avsctx->ctrl= (AVSyncCtrl*)mmap( NULL,
                                       avsctx->ctrlSize,
                                       PROT_READ|PROT_WRITE,
                                       MAP_SHARED | MAP_POPULATE,
                                       avsctx->fd,
                                       0 //offset
                                     );
      if ( avsctx->ctrl == MAP_FAILED )
      {
         GST_ERROR("Error from mmmap for avs control file");
         goto error_exit;
      }

      rc= pthread_mutex_init( &avsctx->ctrl->mutex, &attr);
      if ( rc )
      {
         GST_ERROR("pthread_mutex_init failed: %d", rc);
         goto error_exit;
      }
   }

   count= count+1;

exit:
   return avsctx;

error_exit:
   free( avsctx );
   avsctx= 0;
   goto exit;
}

static void wstDestroyAVSyncCtx( GstWesterosSink *sink, AVSyncCtx *avsctx )
{
   if ( avsctx )
   {
      if ( avsctx->audioSink )
      {
         gst_object_unref( avsctx->audioSink );
         avsctx->audioSink= 0;
      }
      if ( avsctx->ctrl )
      {
         pthread_mutex_destroy( &avsctx->ctrl->mutex );
         munmap( avsctx->ctrl, avsctx->ctrlSize );
         avsctx->ctrl= 0;
      }
      if ( avsctx->fd >= 0 )
      {
         close( avsctx->fd );
         avsctx->fd= -1;
         if ( remove( avsctx->name ) != 0 )
         {
            GST_ERROR("remove failed for avsctx");
         }
      }
      free( avsctx );
   }
}

static void wstUpdateAVSyncCtx( GstWesterosSink *sink, AVSyncCtx *avsctx )
{
   if ( avsctx && avsctx->ctrl )
   {
      if ( avsctx->audioSink )
      {
         long long avTime= 0;
         if ( gst_element_query_position( avsctx->audioSink, GST_FORMAT_TIME, (gint64 *)&avTime ) )
         {
            pthread_mutex_lock( &avsctx->ctrl->mutex );
            avsctx->ctrl->active= (avsctx->ctrl->avTime != avTime/1000LL);
            avsctx->ctrl->sysTime= g_get_monotonic_time();
            avsctx->ctrl->avTime= avTime/1000LL;
            pthread_mutex_unlock( &avsctx->ctrl->mutex );
            GST_LOG("set avTime %lld active %d\n", avsctx->ctrl->avTime, avsctx->ctrl->active);
         }
      }
   }
}
#endif

