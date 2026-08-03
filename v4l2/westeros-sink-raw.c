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

GST_DEBUG_CATEGORY_EXTERN (gst_westeros_sink_debug);
#define GST_CAT_DEFAULT gst_westeros_sink_debug

#define INT_FRAME(FORMAT, ...)      frameLog( "FRAME: " FORMAT "\n", __VA_ARGS__)
#define FRAME(...)                  INT_FRAME(__VA_ARGS__, "")


#define needBounds(sink) ( sink->soc.forceAspectRatio || (sink->soc.zoomMode != ZOOM_NONE) )

enum
{
   SIGNAL_FIRSTFRAME,
   SIGNAL_UNDERFLOW,
   SIGNAL_NEWTEXTURE,
   SIGNAL_DECODEERROR,
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

static bool g_frameDebug= false;
static guint g_signals[MAX_SIGNAL]= {0};

void wstSinkRawStopVideo( GstWesterosSink *sink );
/*Common For RAW and ENCODED*/

void wstSetTextureCropRaw( GstWesterosSink *sink, int vx, int vy, int vw, int vh );
static void wstSendHideVideoClientConnectionRaw( WstVideoClientConnection *conn, bool hide );
static void wstSendSessionInfoVideoClientConnection( WstVideoClientConnection *conn );
void wstSetSessionInfoRaw( GstWesterosSink *sink );
static void wstSendFlushVideoClientConnection( WstVideoClientConnection *conn );
static void wstSendPauseVideoClientConnection( WstVideoClientConnection *conn, bool pause );

static void wstSendRectVideoClientConnection( WstVideoClientConnection *conn );
static void wstSendRateVideoClientConnection( WstVideoClientConnection *conn );
void wstProcessMessagesVideoClientConnectionRaw( WstVideoClientConnection *conn );

bool wstSendFrameVideoClientConnectionRaw( WstVideoClientConnection *conn, int buffIndex );
static unsigned int getU32( unsigned char *p );
static int putU32( unsigned char *p, unsigned n );
static gint64 getS64( unsigned char *p );
static gpointer wstDispatchThread(gpointer data);
static gpointer wstEOSDetectionThread(gpointer data);
static gpointer wstFirstFrameThread(gpointer data);
static gpointer wstUnderflowThread(gpointer data);
static void wstBuildSinkCaps( GstWesterosSinkClass *klass );
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

static int putS64( unsigned char *p, gint64 n )
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

static void buffer_release_raw( void *data, struct wl_buffer *buffer )
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
   buffer_release_raw
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
void wstSetAFDInfo( GstWesterosSink *sink, GstBuffer *buffer )
{
   GstVideoAFDMeta* afd;
   GstVideoBarMeta* bar;
   bool haveNew= false;

   afd= gst_buffer_get_video_afd_meta(buffer);
   bar= gst_buffer_get_video_bar_meta(buffer);

   if ( afd || bar )
   {
      WstAFDInfo *afdCurr= &sink->soc.afdActive;

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
         afdCurr->frameNumber= sink->soc.frameInCount;
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
   memset( &sink->soc.afdActive, 0, sizeof(WstAFDInfo));
}
#endif

void gst_westeros_sink_raw_term( GstWesterosSink *sink )
{
   if ( sink->soc.haveDrmBuffSem )
   {
      sink->soc.haveDrmBuffSem= false;
      sem_destroy( &sink->soc.drmBuffSem );
   }
   #ifdef GLIB_VERSION_2_32
   g_mutex_clear( &sink->soc.mutex );
   #else
   g_mutex_free( sink->soc.mutex );
   #endif
}

gboolean gst_westeros_sink_raw_resource_init( GstWesterosSink *sink, gboolean *passToDefault )
{
   gboolean result= FALSE;

   WESTEROS_UNUSED(passToDefault);
   if ( sinkAcquireResources( sink ) )
   {
      result= TRUE;
   }
   else
   {
      GST_ERROR("gst_westeros_sink_raw_resource_init: sinkAcquireResources failed");
   }

   /* For Raw Caps Event, this should get Init only once. */
   /* As it is called from soc_accept_caps Function, it will get hit for each caps Event */
   /* But we need to do DrmInit only once. So we use a flag in the sink structure to hanlde it*/
   if (result && !sink->soc.isDRMInitDone)
   {
      if ( drmInit( sink ) )
      {
         sink->soc.isDRMInitDone= TRUE; 
      }
      else
      {
         GST_ERROR("gst_westeros_sink_raw_resource_init: drmInit failed");
         result= FALSE;
      }
   }

   return result;
}

gboolean gst_westeros_sink_raw_paused_to_playing( GstWesterosSink *sink, gboolean *passToDefault )
{
   WESTEROS_UNUSED(passToDefault);

   LOCK( sink );
   sink->soc.videoPlaying= TRUE;
   sink->soc.videoPaused= FALSE;
   #ifdef USE_AMLOGIC_MESON_MSYNC
   if ( !sink->soc.userSession )
   #endif
   {
      sink->soc.updateSession= TRUE;
   }
   UNLOCK( sink );
   wstSendPauseVideoClientConnection( sink->soc.conn, false );

   return TRUE;
}

gboolean gst_westeros_sink_raw_playing_to_paused( GstWesterosSink *sink, gboolean *passToDefault )
{
   LOCK( sink );
   sink->soc.videoPlaying= FALSE;
   sink->soc.videoPaused= TRUE;
   UNLOCK( sink );

   wstSendPauseVideoClientConnection( sink->soc.conn, true );

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
   sink->soc.isDRMInitDone= FALSE;

   *passToDefault= false;

   return TRUE;
}

gboolean gst_westeros_sink_raw_setting_capabilities( GstWesterosSink *sink, GstCaps *caps )
{
   bool result= TRUE;
   GstStructure *structure;

   structure= gst_caps_get_structure(caps, 0);
   if( structure )
   {
      gint num, denom, width, height;
      const gchar *format= 0;
      if ( gst_structure_get_fraction( structure, "framerate", &num, &denom ) )
      {
         if ( denom == 0 ) denom= 1;
         sink->soc.frameRate= (double)num/(double)denom;
         if ( sink->soc.frameRate <= 0.0 )
         {
            g_print("westeros-sink: caps have framerate of 0 - assume 60\n");
            sink->soc.frameRate= 60.0;
         }
         if ( (sink->soc.frameRateFractionNum != num) || (sink->soc.frameRateFractionDenom != denom) )
         {
            sink->soc.frameRateFractionNum= num;
            sink->soc.frameRateFractionDenom= denom;
            sink->soc.frameRateChanged= TRUE;
         }
      }
      if ( (sink->soc.frameRate == 0.0) && (sink->soc.frameRateFractionDenom == 0) )
      {
         sink->soc.frameRateFractionDenom= 1;
         sink->soc.frameRateChanged= TRUE;
      }
      sink->soc.pixelAspectRatio= 1.0;
      if ( gst_structure_get_fraction( structure, "pixel-aspect-ratio", &num, &denom ) )
      {
         if ( (num <= 0) || (denom <= 0))
         {
            num= denom= 1;
         }
         sink->soc.pixelAspectRatio= (double)num/(double)denom;
         sink->soc.havePixelAspectRatio= TRUE;
         sink->soc.pixelAspectRatioChanged= TRUE;
      }
      if ( gst_structure_get_int( structure, "width", &width ) )
      {
         sink->soc.frameWidth= width;
         sink->srcWidth= width;
      }
      if ( gst_structure_get_int( structure, "height", &height ) )
      {
         sink->soc.frameHeight= height;
         sink->srcHeight= height;
      }
      format= gst_structure_get_string( structure, "format" );
      if ( format )
      {
         int len= strlen(format);
         if ( (len == 4) && !strncmp( format, "NV12", len) )
         {
            sink->soc.frameFormatStream= DRM_FORMAT_NV12;
         }
         else if ( (len == 4) && !strncmp( format, "NV21", len) )
         {
            sink->soc.frameFormatStream= DRM_FORMAT_NV21;
         }
         else if ( (len == 4) && (!strncmp( format, "I420", len) || !strncmp( format, "YU12", len)) )
         {
            sink->soc.frameFormatStream= DRM_FORMAT_YUV420;
         }
         else
         {
            g_print("format (%s) not supported\n", format);
            result= FALSE;
         }
      }
   }
   else
   {
      GST_DEBUG("westeros-sink: caps have no structure");
       result= FALSE;
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
   haveHardware= sink->soc.haveHardware;
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
      if ( sink->soc.dispatchThread == NULL )
      {
         sink->soc.quitDispatchThread= FALSE;
         GST_DEBUG_OBJECT(sink, "starting westeros_sink_dispatch thread");
         sink->soc.dispatchThread= g_thread_new("westerossinkDSP", wstDispatchThread, sink);
      }
   }

   if ( sink->soc.eosDetectionThread == NULL )
   {
      sink->soc.videoPlaying= TRUE;
      sink->soc.quitEOSDetectionThread= FALSE;
      GST_DEBUG_OBJECT(sink, "starting westeros_sink_eos thread");
      sink->soc.eosDetectionThread= g_thread_new("westerossinkEOS", wstEOSDetectionThread, sink);
   }

   GST_BASE_SINK_PREROLL_UNLOCK(GST_BASE_SINK(sink));
   while ( sink->soc.videoPaused )
   {
      WstVideoClientConnection *conn;
      bool active= true;
      usleep( 1000 );
      LOCK(sink);
      conn= sink->soc.conn;
      if ( conn )
      {
         LOCK_CONN(conn);
      }
      UNLOCK(sink);
      if ( conn )
      {
         wstProcessMessagesVideoClientConnectionRaw( conn );
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

   if ( sink->soc.expectDummyBuffers && !isDmaBuf )
   {
      gint64 frameTime= GST_BUFFER_PTS(buffer);
      gint64 firstNano= ((sink->firstPTS/90LL)*GST_MSECOND)+((sink->firstPTS%90LL)*GST_MSECOND/90LL);
      sink->position= sink->positionSegmentStart + frameTime - firstNano;
      sink->currentPTS= frameTime / (GST_SECOND/90000LL);
      GST_LOG("gst_westeros_sink_raw_render: dummy buffer %p, timestamp: %lld", buffer, GST_BUFFER_PTS(buffer) );
      if ( !sink->soc.conn && (sink->soc.frameOutCount == 0))
      {
         LOCK(sink);
         sink->soc.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
         UNLOCK(sink);
      }
      if ( (sink->soc.frameInCount == 0) && sink->soc.captureEnabled && sink->soc.useTunnelled )
      {
         gst_westeros_sink_raw_set_video_path( sink, true );
      }
      LOCK(sink);
      ++sink->soc.frameInCount;
      ++sink->soc.frameOutCount;
      UNLOCK(sink);
      if ( sink->soc.framesBeforeHideGfx )
      {
         if ( --sink->soc.framesBeforeHideGfx == 0 )
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
         drmBuff= drmGetBuffer( sink, sink->soc.frameWidth, sink->soc.frameHeight );
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
         ++sink->soc.frameInCount;

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
            LOCK(sink);
            if ( nanoTime+duration >= sink->segment.start )
            {
               if ( sink->prevPositionSegmentStart == 0xFFFFFFFFFFFFFFFFLL )
               {
                  sink->soc.currentInputPTS= 0;
               }
               prevPTS= sink->soc.currentInputPTS;
               sink->soc.currentInputPTS= ((nanoTime / GST_SECOND) * 90000)+(((nanoTime % GST_SECOND) * 90000) / GST_SECOND);
               if (sink->prevPositionSegmentStart != sink->positionSegmentStart)
               {
                  sink->firstPTS= sink->soc.currentInputPTS;
                  sink->prevPositionSegmentStart = sink->positionSegmentStart;
                  GST_DEBUG("SegmentStart changed! Updating first PTS to %lld ", sink->firstPTS);
               }
               if ( sink->soc.currentInputPTS != 0 || sink->soc.frameInCount == 0 )
               {
                  if ( (sink->soc.currentInputPTS < sink->firstPTS) && (sink->soc.currentInputPTS > 90000) )
                  {
                     /* If we have hit a discontinuity that doesn't look like rollover, then
                        treat this as the case of looping a short clip.  Adjust our firstPTS
                        to keep our running time correct. */
                     sink->firstPTS= sink->firstPTS-(prevPTS-sink->soc.currentInputPTS);
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
                  wstSetSessionInfoRaw( sink );
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

                  switch( sink->soc.frameFormatStream )
                  {
                     case DRM_FORMAT_NV12:
                     case DRM_FORMAT_NV21:
                        sink->soc.frameFormatOut= sink->soc.frameFormatStream;
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
                           Ystride= ((sink->soc.frameWidth + 3) & ~3);
                           Ustride= Ystride;
                        }
                        Vstride= 0;
                        U= Y + Ystride*sink->soc.frameHeight;
                        V= 0;
                        break;
                     case DRM_FORMAT_YUV420:
                        sink->soc.frameFormatOut= DRM_FORMAT_NV12;
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
                           Ystride= ((sink->soc.frameWidth + 3) & ~3);
                           Ustride= Ystride/2;
                           Vstride= Ystride/2;
                        }
                        U= Y + Ystride*sink->soc.frameHeight;
                        V= U + Ustride*sink->soc.frameHeight/2;
                        break;
                     default:
                        Y= U= V= 0;
                        break;
                  }

                  if ( Y )
                  {
                     data= (unsigned char*)mmap( NULL, drmBuff->size[0], PROT_READ | PROT_WRITE, MAP_SHARED, sink->soc.drmFd, drmBuff->offset[0] );
                     if ( data != MAP_FAILED )
                     {
                        int row;
                        int copyLen= MIN( Ystride, (int)drmBuff->pitch[0] );
                        unsigned char *destRow= data;
                        unsigned char *srcYRow= Y;
                        for( row= 0; row < sink->soc.frameHeight; ++row )
                        {
                           memcpy( destRow, srcYRow, copyLen );
                           destRow += drmBuff->pitch[0];
                           srcYRow += Ystride;
                        }
                        munmap( data, drmBuff->size[0] );
                     }
                     else
                     {
                         GST_ERROR("mmap failed for Y plane: errno %d", errno);
                     }
                     if ( U && !V )
                     {
                        data= (unsigned char*)mmap( NULL, drmBuff->size[1], PROT_READ | PROT_WRITE, MAP_SHARED, sink->soc.drmFd, drmBuff->offset[1] );
                        if ( data != MAP_FAILED )
                        {
                           int row;
                           int copyLen= MIN( Ustride, (int)drmBuff->pitch[1] );
                           unsigned char *destRow= data;
                           unsigned char *srcURow= U;
                           for( row= 0; row < sink->soc.frameHeight; row += 2 )
                           {
                              memcpy( destRow, srcURow, copyLen );
                              destRow += drmBuff->pitch[1];
                              srcURow += Ustride;
                           }
                           munmap( data, drmBuff->size[1] );
                        }
                        else
                        {
                           GST_ERROR("mmap failed for UV plane: errno %d", errno);
                        }
                     }
                     if ( U && V )
                     {
                        int bi;
                        int bufferUOffset;
                        #ifdef USE_SINGLE_BUFFER_NV12
                        bi= 0;
                        bufferUOffset= Ystride*sink->soc.frameHeight;
                        #else
                        bi= 1;
                        bufferUOffset= 0;
                        #endif
                        data= (unsigned char*)mmap( NULL, drmBuff->size[bi], PROT_READ | PROT_WRITE, MAP_SHARED, sink->soc.drmFd, drmBuff->offset[bi] );
                        if ( data != MAP_FAILED )
                        {
                           int row, col;
                           unsigned char *dest, *destRow= data + bufferUOffset;
                           unsigned char *srcU, *srcURow= U;
                           unsigned char *srcV, *srcVRow= V;
                           for( row= 0; row < sink->soc.frameHeight; row += 2 )
                           {
                              dest= destRow;
                              srcU= srcURow;
                              srcV= srcVRow;
                              for( col= 0; col < sink->soc.frameWidth; col += 2 )
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
                        else
                        {
                           GST_ERROR("mmap failed for UV interleave plane: errno %d", errno);
                        }
                     }
                  }
               }

               if ( !sink->soc.conn && (sink->soc.frameOutCount == 0))
               {
                  LOCK(sink);
                  sink->soc.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
                  UNLOCK(sink);
               }

               drmBuff->frameTime= ((GST_BUFFER_PTS(buffer) + 500LL) / 1000LL);

               if ( !sink->soc.conn )
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

               if ( sink->soc.enableTextureSignal )
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
                                 sink->soc.frameFormatOut,
                                 sink->soc.frameWidth,
                                 sink->soc.frameHeight,
                                 fd0, l0, s0, p0,
                                 fd1, l1, s1, p1,
                                 fd2, l2, s2, p2
                               );
               }
               else if ( sink->soc.captureEnabled && sink->soc.sb && sink->show )
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
                     pixelFormat= (sink->soc.frameFormatOut == DRM_FORMAT_NV12) ? WL_SB_FORMAT_NV12 : WL_SB_FORMAT_NV21;

                     binfo->sink= sink;
                     binfo->buffIndex= buffIndex;
                     wlbuff= wl_sb_create_planar_buffer_fd2( sink->soc.sb,
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
                        sink->soc.resubFd= sink->soc.prevFrame2Fd;
                        sink->soc.prevFrame2Fd=sink->soc.prevFrame1Fd;
                        sink->soc.prevFrame1Fd= sink->soc.nextFrameFd;
                        sink->soc.nextFrameFd= -1;

                        if ( sink->soc.framesBeforeHideVideo )
                        {
                           if ( --sink->soc.framesBeforeHideVideo == 0 )
                           {
                              wstSendHideVideoClientConnectionRaw( sink->soc.conn, true );
                           }
                        }
                     }
                     else
                     {
                        free( binfo );
                     }
                  }
               }
               if ( sink->soc.conn )
               {
                  if ( sink->soc.expectDummyBuffers )
                  {
                     buffIndex= -1;
                  }
                  else
                  {
                     if ( sink->soc.showChanged )
                     {
                        sink->soc.showChanged= FALSE;
                        if ( !sink->soc.captureEnabled )
                        {
                           wstSendHideVideoClientConnectionRaw( sink->soc.conn, !sink->show );
                        }
                     }
                     if ( sink->soc.frameRateChanged )
                     {
                        sink->soc.frameRateChanged= FALSE;
                        wstSendRateVideoClientConnection( sink->soc.conn );
                     }
                     sink->soc.resubFd= sink->soc.prevFrame2Fd;
                     sink->soc.prevFrame2Fd= sink->soc.prevFrame1Fd;
                     sink->soc.prevFrame1Fd= sink->soc.nextFrameFd;
                     sink->soc.nextFrameFd= sink->soc.drmBuffer[buffIndex].fd[0];
                     if ( wstSendFrameVideoClientConnectionRaw( sink->soc.conn, buffIndex ) )
                     {
                        buffIndex= -1;
                     }

                     if ( sink->soc.framesBeforeHideGfx )
                     {
                        if ( --sink->soc.framesBeforeHideGfx == 0 )
                        {
                           wl_surface_attach( sink->surface, 0, sink->windowX, sink->windowY );
                           wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
                           wl_surface_commit( sink->surface );
                           wl_display_flush(sink->display);
                           wl_display_dispatch_queue_pending(sink->display, sink->queue);
                           if ( sink->show )
                           {
                              wstSendHideVideoClientConnectionRaw( sink->soc.conn, false );
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
         ++sink->soc.frameOutCount;
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
      wstSendFlushVideoClientConnection( sink->soc.conn );
      sink->startAfterCaps= TRUE;
      sink->soc.prevFrameTimeGfx= 0;
      sink->soc.prevFramePTSGfx= 0;
      sink->soc.prevFrame1Fd= -1;
      sink->soc.prevFrame2Fd= -1;
      sink->soc.nextFrameFd= -1;
   }
   LOCK(sink);
   sink->soc.frameInCount= 0;
   sink->soc.frameOutCount= 0;
   sink->soc.frameDisplayCount= 0;
   sink->soc.numDropped= 0;
   #ifdef USE_GST_AFD
   wstFlushAFDInfo( sink );
   #endif
   UNLOCK(sink);
}

void gst_westeros_sink_raw_set_video_path( GstWesterosSink *sink, bool useGfxPath )
{
   if ( useGfxPath && !sink->soc.captureEnabled )
   {
      sink->soc.captureEnabled= TRUE;

      sink->soc.framesBeforeHideVideo= sink->soc.hideVideoFramesDelay;
   }
   else if ( !useGfxPath && sink->soc.captureEnabled )
   {
      sink->soc.captureEnabled= FALSE;
      sink->soc.prevFrame1Fd= -1;
      sink->soc.prevFrame2Fd= -1;
      sink->soc.nextFrameFd= -1;
      sink->soc.framesBeforeHideGfx= sink->soc.hideGfxFramesDelay;
   }
   if ( sink->soc.useTunnelled )
   {
      GstPad *pad= GST_BASE_SINK(sink)->sinkpad;
      if ( pad )
      {
         GstStructure *structure;
         int vx, vy, vw, vh;
         guint gfxpath= sink->soc.captureEnabled ? 1 : 0;
         if ( sink->soc.captureEnabled )
         {
            vx= 0;
            vy= 0;
            vw= 1;
            vh= 1;
         }
         else
         {
            vx= sink->soc.videoX;
            vy= sink->soc.videoY;
            vw= sink->soc.videoWidth;
            vh= sink->soc.videoHeight;
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
      tx= sink->soc.videoX;
      ty= sink->soc.videoY;
      tw= sink->soc.videoWidth;
      th= sink->soc.videoHeight;
      sink->soc.videoX= sink->windowX;
      sink->soc.videoY= sink->windowY;
      sink->soc.videoWidth= sink->windowWidth;
      sink->soc.videoHeight= sink->windowHeight;

      wstGetVideoBoundsRaw( sink, &vx, &vy, &vw, &vh );
      wstSetTextureCropRaw( sink, vx, vy, vw, vh );

      sink->soc.videoX= tx;
      sink->soc.videoY= ty;
      sink->soc.videoWidth= tw;
      sink->soc.videoHeight= th;
   }
}

void gst_westeros_sink_raw_update_video_position( GstWesterosSink *sink )
{
   bool needUpdate= true;
   int vx, vy, vw, vh;
   vx= sink->soc.videoX;
   vy= sink->soc.videoY;
   vw= sink->soc.videoWidth;
   vh= sink->soc.videoHeight;

   if ( sink->windowSizeOverride )
   {
      sink->soc.videoX= ((sink->windowX*sink->scaleXNum)/sink->scaleXDenom) + sink->transX;
      sink->soc.videoY= ((sink->windowY*sink->scaleYNum)/sink->scaleYDenom) + sink->transY;
      sink->soc.videoWidth= (sink->windowWidth*sink->scaleXNum)/sink->scaleXDenom;
      sink->soc.videoHeight= (sink->windowHeight*sink->scaleYNum)/sink->scaleYDenom;
   }
   else
   {
      sink->soc.videoX= sink->transX;
      sink->soc.videoY= sink->transY;
      sink->soc.videoWidth= (sink->outputWidth*sink->scaleXNum)/sink->scaleXDenom;
      sink->soc.videoHeight= (sink->outputHeight*sink->scaleYNum)/sink->scaleYDenom;
   }

   if ( (vx == sink->soc.videoX) && (vy == sink->soc.videoY) &&
        (vw == sink->soc.videoWidth) && (vh == sink->soc.videoHeight) )
   {
      needUpdate= false;
   }

   if ( !sink->soc.captureEnabled && needUpdate )
   {
      /* Send a buffer to compositor to update hole punch geometry */
      if ( sink->soc.sb )
      {
         struct wl_buffer *buff;

         buff= wl_sb_create_buffer( sink->soc.sb,
                                    0,
                                    sink->windowWidth,
                                    sink->windowHeight,
                                    sink->windowWidth*4,
                                    WL_SB_FORMAT_ARGB8888 );
         wl_surface_attach( sink->surface, buff, sink->windowX, sink->windowY );
         wl_surface_damage( sink->surface, 0, 0, sink->windowWidth, sink->windowHeight );
         wl_surface_commit( sink->surface );
      }
      if ( sink->soc.videoPaused )
      {
         wstSendRectVideoClientConnection(sink->soc.conn);
      }
      if ( sink->soc.useTunnelled )
      {
         GstPad *pad= GST_BASE_SINK(sink)->sinkpad;
         if ( pad )
         {
            GstStructure *structure;
            structure= gst_structure_new("westeros-raw-rectangle",
                                         "res-width", G_TYPE_UINT, sink->displayWidth,
                                         "res-height", G_TYPE_UINT, sink->displayHeight,
                                         "rectx", G_TYPE_INT, sink->soc.videoX,
                                         "recty", G_TYPE_INT, sink->soc.videoY,
                                         "rectw", G_TYPE_INT, sink->soc.videoWidth,
                                         "recth", G_TYPE_INT, sink->soc.videoHeight,
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

void wstSinkRawStopVideo( GstWesterosSink *sink )
{
   LOCK(sink);
   sink->videoStarted= FALSE;
   if ( sink->soc.conn )
   {
      wstDestroyVideoClientConnection( sink->soc.conn );
      sink->soc.conn= 0;
   }
   if ( sink->soc.eosDetectionThread || sink->soc.dispatchThread )
   {
      sink->soc.quitEOSDetectionThread= TRUE;
      sink->soc.quitDispatchThread= TRUE;
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

   sink->soc.prevFrame1Fd= -1;
   sink->soc.prevFrame2Fd= -1;
   sink->soc.nextFrameFd= -1;
   sink->soc.frameWidth= -1;
   sink->soc.frameHeight= -1;
   sink->soc.frameRate= 0.0;
   sink->soc.frameRateFractionNum= 0;
   sink->soc.frameRateFractionDenom= 0;
   sink->soc.pixelAspectRatio= 1.0;
   sink->soc.havePixelAspectRatio= FALSE;
   sink->soc.syncType= -1;
   sink->soc.emitFirstFrameSignal= FALSE;
   sink->soc.emitUnderflowSignal= FALSE;

   LOCK(sink);
   sink->videoStarted= FALSE;
   #ifdef USE_GST_AFD
   wstFlushAFDInfo( sink);
   #endif
   UNLOCK(sink);

   if ( sink->soc.eosDetectionThread )
   {
      sink->soc.quitEOSDetectionThread= TRUE;
      g_thread_join( sink->soc.eosDetectionThread );
      sink->soc.eosDetectionThread= NULL;
   }

   if ( sink->soc.dispatchThread )
   {
      sink->soc.quitDispatchThread= TRUE;
      g_thread_join( sink->soc.dispatchThread );
      sink->soc.dispatchThread= NULL;
   }

   #ifdef USE_GENERIC_AVSYNC
   if ( sink->soc.avsctx )
   {
      wstDestroyAVSyncCtx( sink, sink->soc.avsctx );
      sink->soc.avsctx= 0;
   }
   #endif

   if ( sink->soc.sb )
   {
      wl_sb_destroy( sink->soc.sb );
      sink->soc.sb= 0;
   }

   drmTerm( sink );
}

void wstGetVideoBoundsRaw( GstWesterosSink *sink, int *x, int *y, int *w, int *h )
{
   int vx, vy, vw, vh;
   int frameWidth, frameHeight;
   int zoomMode;;
   double contentWidth, contentHeight;
   double roix, roiy, roiw, roih;
   double arf, ard;
   double hfactor= 1.0, vfactor= 1.0;
   vx= sink->soc.videoX;
   vy= sink->soc.videoY;
   vw= sink->soc.videoWidth;
   vh= sink->soc.videoHeight;

   if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("pixelAspectRatio: %f zoom-mode %d overscan-size %d", sink->soc.pixelAspectRatio, sink->soc.zoomMode, sink->soc.overscanSize );
   frameWidth= sink->soc.frameWidth;
   frameHeight= sink->soc.frameHeight;
   contentWidth= frameWidth*sink->soc.pixelAspectRatio;
   contentHeight= frameHeight;
   if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("frame %dx%d contentWidth: %f contentHeight %f", frameWidth, frameHeight, contentWidth, contentHeight );
   ard= (double)sink->soc.videoWidth/(double)sink->soc.videoHeight;
   arf= (double)contentWidth/(double)contentHeight;

   /* Establish region of interest */
   roix= 0;
   roiy= 0;
   roiw= contentWidth;
   roih= contentHeight;

   zoomMode= sink->soc.zoomMode;
   if ( !sink->soc.allow4kZoom &&
        ((sink->soc.frameWidth > 1920) || (sink->soc.frameHeight > 1080)) )
   {
      zoomMode= ZOOM_NORMAL;
      if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("4k (%dx%d) force zoom mormal", sink->soc.frameWidth, sink->soc.frameHeight);
   }
   if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("ard %f arf %f", ard, arf);
   switch( zoomMode )
   {
      case ZOOM_NORMAL:
         {
            if ( arf >= ard )
            {
               vw= sink->soc.videoWidth * (1.0+(2.0*sink->soc.overscanSize/100.0));
               vh= (roih * vw) / roiw;
               vx= vx+(sink->soc.videoWidth-vw)/2;
               vy= vy+(sink->soc.videoHeight-vh)/2;
            }
            else
            {
               vh= sink->soc.videoHeight * (1.0+(2.0*sink->soc.overscanSize/100.0));
               vw= (roiw * vh) / roih;
               vx= vx+(sink->soc.videoWidth-vw)/2;
               vy= vy+(sink->soc.videoHeight-vh)/2;
            }
         }
         break;
      case ZOOM_NONE:
      case ZOOM_DIRECT:
         {
            if ( arf >= ard )
            {
               vh= (contentHeight * sink->soc.videoWidth) / contentWidth;
               vy= vy+(sink->soc.videoHeight-vh)/2;
            }
            else
            {
               vw= (contentWidth * sink->soc.videoHeight) / contentHeight;
               vx= vx+(sink->soc.videoWidth-vw)/2;
            }
         }
         break;
      case ZOOM_16_9_STRETCH:
         {
            if ( wstApproxEqual(arf, ard) && wstApproxEqual(arf, 1.777) )
            {
               /* For 16:9 content on a 16:9 display, stretch as though 4:3 */
               hfactor= 4.0/3.0;
               if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("stretch apply vfactor %f hfactor %f", vfactor, hfactor);
            }
            vh= sink->soc.videoHeight * (1.0+(2.0*sink->soc.overscanSize/100.0));
            vw= vh*16/9;
            vx= vx+(sink->soc.videoWidth-vw)/2;
            vy= vy+(sink->soc.videoHeight-vh)/2;
         }
         break;
      case ZOOM_4_3_PILLARBOX:
         {
            vh= sink->soc.videoHeight * (1.0+(2.0*sink->soc.overscanSize/100.0));
            vw= vh*4/3;
            vx= vx+(sink->soc.videoWidth-vw)/2;
            vy= vy+(sink->soc.videoHeight-vh)/2;
         }
         break;
      case ZOOM_ZOOM:
         {
            #ifdef USE_GST_AFD
            /* Adjust region of interest based on AFD+Bars */
            if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("afd %d haveBar %d isLetterbox %d d1 %d d2 %d", sink->soc.afdActive.afd, sink->soc.afdActive.haveBar,
                                                                sink->soc.afdActive.isLetterbox, sink->soc.afdActive.d1, sink->soc.afdActive.d2 );
            switch ( sink->soc.afdActive.afd )
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
                  if ( sink->soc.afdActive.haveBar )
                  {
                     int activeHeight= roih-sink->soc.afdActive.d1;
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
                  if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("zoom apply vfactor %f hfactor %f", vfactor, hfactor);
               }
               vh= sink->soc.videoHeight * vfactor * (1.0+(2.0*sink->soc.overscanSize/100.0));
               vw= (roiw * vh) * hfactor / roih;
               vx= vx+(sink->soc.videoWidth-vw)/2;
               vy= vy+(sink->soc.videoHeight-vh)/2;
            }
            else
            {
               vw= sink->soc.videoWidth * (1.0+(2.0*sink->soc.overscanSize/100.0));
               vh= (roih * vw) / roiw;
               vx= vx+(sink->soc.videoWidth-vw)/2;
               vy= vy+(sink->soc.videoHeight-vh)/2;
            }
         }
         break;
   }
   if ( sink->soc.pixelAspectRatioChanged ) GST_DEBUG("vrect %d, %d, %d, %d", vx, vy, vw, vh);
   if ( sink->soc.pixelAspectRatioChanged )
   {
      if ( sink->display && sink->vpcSurface )
      {
         if ( sink->soc.captureEnabled || sink->soc.framesBeforeHideGfx )
         {
            wl_vpc_surface_set_geometry( sink->vpcSurface, vx, vy, vw, vh );
         }
         else
         {
            wl_vpc_surface_set_geometry( sink->vpcSurface, sink->windowX, sink->windowY, sink->windowWidth, sink->windowHeight );
         }
         wl_display_flush(sink->display);
      }
      else
      {
         GST_WARNING("wstGetVideoBoundsRaw: pixelAspectRatioChanged but display %p vpcSurface %p",
                      sink->display,
                      sink->vpcSurface);
      }
   }
   sink->soc.pixelAspectRatioChanged= FALSE;
   *x= vx;
   *y= vy;
   *w= vw;
   *h= vh;
}

void wstSetTextureCropRaw( GstWesterosSink *sink, int vx, int vy, int vw, int vh )
{
   GST_DEBUG("wstSetTextureCropRaw: vx %d vy %d vw %d vh %d window(%d, %d, %d, %d) display(%dx%d)",
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
         GST_LOG("wstSetTextureCropRaw: CX1");
         cropx= (sink->windowX-vx)*sink->windowWidth/vw;
         cropw= (sink->windowX+sink->windowWidth-vx)*sink->windowWidth/vw - cropx;
      }
      else if ( vx < 0 )
      {
         GST_LOG("wstSetTextureCropRaw: CX2");
         cropx= -vx*sink->windowWidth/vw;
         cropw= (vw+vx)*sink->windowWidth/vw;
      }
      else if ( vx+vw > sink->windowWidth )
      {
         GST_LOG("wstSetTextureCropRaw: CX3");
         cropx= 0;
         cropw= (sink->windowWidth-vx)*sink->windowWidth/vw;
      }

      if ( (vy < sink->windowY) || (vy+vh > sink->windowY+sink->windowHeight) )
      {
         GST_LOG("wstSetTextureCropRaw: CY1");
         cropy= (sink->windowY-vy)*sink->windowHeight/vh;
         croph= (sink->windowY+sink->windowHeight-vy)*sink->windowHeight/vh - cropy;
      }
      else if ( vy < 0 )
      {
         GST_LOG("wstSetTextureCropRaw: CY2");
         cropy= -vy*sink->windowHeight/vh;
         croph= (vh+vy)*sink->windowHeight/vh;
      }
      else if ( vy+vh > sink->windowHeight )
      {
         GST_LOG("wstSetTextureCropRaw: CY3");
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
         GST_LOG("wstSetTextureCropRaw: WX1");
         vx= wx1;
      }
      else if ( (wx1 >= vx) && (wx1 < 0) )
      {
         GST_LOG("wstSetTextureCropRaw: WX2");
         vw += wx1;
         vx= 0;
      }
      else if ( wx2 < vx+vw )
      {
         GST_LOG("wstSetTextureCropRaw: WX3");
         vw= wx2-vx;
      }
      if ( (wx1 >= 0) && (wx2 > vw) )
      {
         GST_LOG("wstSetTextureCropRaw: WX4");
         vw= vw-wx1;
      }
      else if ( wx2 < vx+vw )
      {
         GST_LOG("wstSetTextureCropRaw: WX5");
         vw= wx2-vx;
      }

      if ( (wy1 > vy) && (wy1 > 0) )
      {
         GST_LOG("wstSetTextureCropRaw: WY1");
         vy= wy1;
      }
      else if ( (wy1 >= vy) && (wy1 < 0) )
      {
         GST_LOG("wstSetTextureCropRaw: WY2");
         vy= 0;
      }
      else if ( (wy1 < vy) && (wy1 > 0) )
      {
         GST_LOG("wstSetTextureCropRaw: WY3");
         vh -= wy1;
      }
      if ( (wy1 >= 0) && (wy2 > vh) )
      {
         GST_LOG("wstSetTextureCropRaw: WY4");
         vh= vh-wy1;
      }
      else if ( wy2 < vy+vh )
      {
         GST_LOG("wstSetTextureCropRaw: WY5");
         vh= wy2-vy;
      }
      if ( vw < 0 ) vw= 0;
      if ( vh < 0 ) vh= 0;
      cropx= (cropx*WL_VPC_SURFACE_CROP_DENOM)/sink->windowWidth;
      cropy= (cropy*WL_VPC_SURFACE_CROP_DENOM)/sink->windowHeight;
      cropw= (cropw*WL_VPC_SURFACE_CROP_DENOM)/sink->windowWidth;
      croph= (croph*WL_VPC_SURFACE_CROP_DENOM)/sink->windowHeight;
      GST_DEBUG("wstSetTextureCropRaw: %d, %d, %d, %d - %d, %d, %d, %d\n", vx, vy, vw, vh, cropx, cropy, cropw, croph);
      wl_vpc_surface_set_geometry_with_crop( sink->vpcSurface, vx, vy, vw, vh, cropx, cropy, cropw, croph );
   }
   else
   {
      if ( sink->soc.captureEnabled || sink->soc.framesBeforeHideGfx )
      {
         wl_vpc_surface_set_geometry( sink->vpcSurface, vx, vy, vw, vh );
      }
      else
      {
         wl_vpc_surface_set_geometry( sink->vpcSurface, sink->windowX, sink->windowY, sink->windowWidth, sink->windowHeight );
      }
   }
}

static void wstSendHideVideoClientConnectionRaw( WstVideoClientConnection *conn, bool hide )
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

static void wstSendSessionInfoVideoClientConnection( WstVideoClientConnection *conn )
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
      mbody[len++]= sink->soc.syncType;
      len += putU32( &mbody[len], conn->sink->soc.sessionId );
      #ifdef USE_GENERIC_AVSYNC
      if ( sink->soc.avsctx )
      {
         fdToSend= fcntl( sink->soc.avsctx->fd, F_DUPFD_CLOEXEC, 0 );
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

            len += putU32( &mbody[len], sink->soc.avsctx->ctrlSize );
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
         GST_DEBUG("sent session info: type %d sessionId %d to video server", sink->soc.syncType, sink->soc.sessionId);
         g_print("sent session info: type %d sessionId %d to video server\n", sink->soc.syncType, sink->soc.sessionId);
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

void wstSetSessionInfoRaw( GstWesterosSink *sink )
{
   #if defined USE_AMLOGIC_MESON || defined USE_GENERIC_AVSYNC
   if ( sink->soc.conn )
   {
      GstElement *audioSink;
      GstElement *element= GST_ELEMENT(sink);
      GstClock *clock= GST_ELEMENT_CLOCK(element);
      int syncTypePrev= sink->soc.syncType;
      int sessionIdPrev= sink->soc.sessionId;
      #ifdef USE_AMLOGIC_MESON_MSYNC
      if ( sink->soc.userSession )
      {
         syncTypePrev= -1;
         sessionIdPrev= -1;
      }
      else
      {
         sink->soc.syncType= 0;
         sink->soc.sessionId= INVALID_SESSION_ID;
         audioSink= wstFindAudioSink( sink );
         if ( audioSink )
         {
            GstClock* amlclock= gst_aml_hal_asink_get_clock( audioSink );
            if (amlclock)
            {
               sink->soc.syncType= 1;
               sink->soc.sessionId= gst_aml_clock_get_session_id( amlclock );
               gst_object_unref( amlclock );
            }
            else
            {
               GST_WARNING ("no clock: vmaster mode");
            }
            gst_object_unref( audioSink );
            GST_WARNING("AmlHalAsink detected, sesison_id: %d", sink->soc.sessionId);
         }
      }
      #else
      sink->soc.syncType= 0;
      sink->soc.sessionId= 0;
      audioSink= wstFindAudioSink( sink );
      if ( audioSink )
      {
         sink->soc.syncType= 1;
         #ifdef USE_GENERIC_AVSYNC
         if ( !gst_base_sink_get_sync(GST_BASE_SINK(sink)) )
         {
            if ( sink->soc.avsctx && (sink->soc.avsctx->audioSink != audioSink) )
            {
               wstDestroyAVSyncCtx( sink, sink->soc.avsctx );
               sink->soc.avsctx= 0;
            }
            if ( !sink->soc.avsctx )
            {
               sink->soc.avsctx= wstCreateAVSyncCtx( sink );
               syncTypePrev= -1;
            }
            if ( sink->soc.avsctx )
            {
               sink->soc.avsctx->audioSink= (GstElement*)gst_object_ref(audioSink);
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
               sink->soc.syncType= 1;
               /* TBD: set sessionid */
            }
            g_free( clockName );
         }
      }
      if ( sink->resAssignedId >= 0 )
      {
         sink->soc.sessionId= sink->resAssignedId;
      }
      #endif
      if ( (syncTypePrev != sink->soc.syncType) || (sessionIdPrev != sink->soc.sessionId) )
      {
         wstSendSessionInfoVideoClientConnection( sink->soc.conn );
      }
   }
   #endif
}

static void wstSendFlushVideoClientConnection( WstVideoClientConnection *conn )
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

static void wstSendPauseVideoClientConnection( WstVideoClientConnection *conn, bool pause )
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

static void wstSendRectVideoClientConnection( WstVideoClientConnection *conn )
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

      vx= sink->soc.videoX;
      vy= sink->soc.videoY;
      vw= sink->soc.videoWidth;
      vh= sink->soc.videoHeight;
      if ( needBounds(sink) )
      {
         wstGetVideoBoundsRaw( sink, &vx, &vy, &vw, &vh );
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

static void wstSendRateVideoClientConnection( WstVideoClientConnection *conn )
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
      len += putU32( &mbody[len], sink->soc.frameRateFractionNum );
      len += putU32( &mbody[len], sink->soc.frameRateFractionDenom );

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

bool wstAuthenticateVideoClientConnection( WstVideoClientConnection *conn )
{
   bool result= false;

   if ( conn )
   {
      GstWesterosSink *sink= conn->sink;
      struct msghdr msg;
      struct iovec iov[1];
      struct pollfd pfd;
      unsigned char mbody[8];
      int len;
      int sentLen;
      int rc;
      int attempts;

      if ( !sink->soc.haveDrmAuthMagic )
      {
         GST_ERROR("wstAuthenticateVideoClientConnection: no DRM auth magic available");
         goto exit;
      }

      conn->drmAuthReplyReceived= FALSE;
      conn->drmAuthReplyRc= -1;
      conn->drmAuthReplyErr= 0;

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
      mbody[len++]= 5;
      mbody[len++]= 'O';
      mbody[len++]= (sink->soc.drmAuthMagic >> 24) & 0xFF;
      mbody[len++]= (sink->soc.drmAuthMagic >> 16) & 0xFF;
      mbody[len++]= (sink->soc.drmAuthMagic >> 8) & 0xFF;
      mbody[len++]= sink->soc.drmAuthMagic & 0xFF;
	  
	  GST_DEBUG("drmAuthMagic: dec=%u hex=0x%08X",
            (unsigned int)sink->soc.drmAuthMagic,
            (unsigned int)sink->soc.drmAuthMagic);

      iov[0].iov_base= (char*)mbody;
      iov[0].iov_len= len;

      do
      {
         sentLen= sendmsg( conn->socketFd, &msg, MSG_NOSIGNAL );
      }
      while ( (sentLen < 0) && (errno == EINTR));
      UNLOCK_CONN( conn );

      if ( sentLen != len )
      {
         GST_ERROR("wstAuthenticateVideoClientConnection: send auth request failed: errno %d", errno);
         goto exit;
      }

      pfd.fd= conn->socketFd;
      pfd.events= POLLIN;
      pfd.revents= 0;

      for( attempts= 0; attempts < 20; ++attempts )
      {
         do
         {
            rc= poll( &pfd, 1, 100 );
         }
         while ( (rc < 0) && (errno == EINTR) );

         if ( rc < 0 )
         {
            GST_ERROR("wstAuthenticateVideoClientConnection: poll failed: errno %d", errno);
            goto exit;
         }
         if ( rc == 0 )
         {
            continue;
         }
         if ( pfd.revents & (POLLERR|POLLHUP|POLLNVAL) )
         {
            GST_ERROR("wstAuthenticateVideoClientConnection: socket error revents 0x%x", pfd.revents);
            goto exit;
         }
         if ( pfd.revents & POLLIN )
         {
            wstProcessMessagesVideoClientConnectionRaw( conn );
            if ( conn->drmAuthReplyReceived )
            {
               break;
            }
         }
      }

      if ( !conn->drmAuthReplyReceived )
      {
         GST_ERROR("wstAuthenticateVideoClientConnection: timeout waiting for auth response");
         goto exit;
      }
      if ( conn->drmAuthReplyRc != 0 )
      {
         GST_ERROR("wstAuthenticateVideoClientConnection: auth failed status %d errno %d",
                   conn->drmAuthReplyRc,
                   conn->drmAuthReplyErr);
         goto exit;
      }

      sink->soc.drmAuthenticated= TRUE;
      result= true;
   }

exit:
   return result;
}

void wstProcessMessagesVideoClientConnectionRaw( WstVideoClientConnection *conn )
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
               id= m[3];
               if ( len >= (mlen+3) )
               {
                  //id= m[3];
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
                           if ( (bi < 0) || (bi >= WST_NUM_DRM_BUFFERS) )
                           {
                              GST_ERROR("release received for invalid buffer index %d", bi );
                           }
                           else if ( sink->soc.drmBuffer[bi].locked )
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
                           sink->soc.numDropped= getU32( &m[12] );
                           FRAME( "out:       status received: frameTime %lld numDropped %d", frameTime, sink->soc.numDropped);
                           if ( (frameTime != -1LL) && (sink->prevPositionSegmentStart != 0xFFFFFFFFFFFFFFFFLL) )
                           {
                              gint64 currentNano= frameTime*1000LL;
                              gint64 firstNano= ((sink->firstPTS/90LL)*GST_MSECOND)+((sink->firstPTS%90LL)*GST_MSECOND/90LL);
                              sink->position= sink->positionSegmentStart + currentNano - firstNano;
                              sink->currentPTS= currentNano / (GST_SECOND/90000LL);
                              GST_LOG("receive frameTime: %lld position %lld", currentNano, sink->position);
                              if (sink->soc.frameDisplayCount == 0)
                              {
                                  sink->soc.emitFirstFrameSignal= TRUE;
                              }
                              ++sink->soc.frameDisplayCount;
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
                              sink->soc.emitUnderflowSignal= TRUE;
                           }
                        }
                        break;
                     case 'Z':
                        if ( mlen >= 13)
                        {
                          int globalZoomActive= getU32( &m[4] );
                          int allow4kZoom= getU32( &m[8] );
                          int zoomMode= getU32( &m[12] );
                          GST_DEBUG("got zoom-mode %d from video server (globalZoomActive %d allow4kZoom %d)", zoomMode,globalZoomActive,allow4kZoom);
                          if ( sink->soc.zoomModeUser == -1 )
                          {
                             sink->soc.zoomModeGlobal= globalZoomActive;
                             if ( !globalZoomActive )
                             {
                                sink->soc.zoomMode= ZOOM_NONE;
                             }
                          }
                          sink->soc.allow4kZoom= allow4kZoom;
                          if ( sink->soc.zoomModeGlobal == TRUE )
                          {
                             if ( (zoomMode >= ZOOM_NONE) && (zoomMode <= ZOOM_ZOOM) )
                             {
                                sink->soc.zoomMode= zoomMode;
                                sink->soc.pixelAspectRatioChanged= TRUE;
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
                     case 'Q':
                        if ( mlen >= 9 )
                        {
                           conn->drmAuthReplyRc= (int)(((unsigned int)m[4] << 24) |
                                                       ((unsigned int)m[5] << 16) |
                                                       ((unsigned int)m[6] << 8) |
                                                       (unsigned int)m[7]);
                           conn->drmAuthReplyErr= (int)(((unsigned int)m[8] << 24) |
                                                        ((unsigned int)m[9] << 16) |
                                                        ((unsigned int)m[10] << 8) |
                                                        (unsigned int)m[11]);
                           conn->drmAuthReplyReceived= TRUE;
                           GST_DEBUG("got drm auth reply status %d errno %d",
                                     conn->drmAuthReplyRc,
                                     conn->drmAuthReplyErr);
                        }
                        break;
                     default:
                        GST_WARNING("wstProcessMessagesVideoClientConnectionRaw: unhandled message id %c(0x%02X) mlen %d",
                                     ((id >= 32) && (id <= 126)) ? id : '?',
                                     id,
                                     mlen);
                        break;
                  }
                  m += (mlen+3);
                  len -= (mlen+3);
               }
               else
               {
                  GST_WARNING("wstProcessMessagesVideoClientConnectionRaw: incomplete message id %c(0x%02X) mlen %d remainingLen %d",
                               ((id >= 32) && (id <= 126)) ? id : '?',
                               id,
                               mlen,
                               len);
                  len= 0;
               }
            }
            else
            {
               GST_WARNING("wstProcessMessagesVideoClientConnectionRaw: invalid message header %02X %02X remainingLen %d",
                            m[0], m[1], len);
               len= 0;
            }
         }
         if ( sink->soc.emitFirstFrameSignal )
         {
            sink->soc.emitFirstFrameSignal= FALSE;
            LOCK(sink);
            sink->soc.firstFrameThread= g_thread_new("westerossinkFFr", wstFirstFrameThread, sink);
            UNLOCK(sink);
         }
         if ( sink->soc.emitUnderflowSignal )
         {
            sink->soc.emitUnderflowSignal= FALSE;
            LOCK(sink);
            sink->soc.underflowThread= g_thread_new("westerossinkUF", wstUnderflowThread, sink);
            UNLOCK(sink);
         }
      }
      else if ( rc < 0 )
      {
         GST_ERROR("wstProcessMessagesVideoClientConnectionRaw: poll failed errno %d", errno);
      }
      else
      {
         GST_DEBUG("wstProcessMessagesVideoClientConnectionRaw: no pending message from video server");
      }
   }
   else
   {
      GST_WARNING("wstProcessMessagesVideoClientConnectionRaw: conn is NULL");
   }
}

bool wstSendFrameVideoClientConnectionRaw( WstVideoClientConnection *conn, int buffIndex )
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

      LOCK_CONN( conn );
      wstProcessMessagesVideoClientConnectionRaw( conn );
      UNLOCK_CONN( conn );

      if ( buffIndex >= 0 )
      {
         sink->soc.resubFd= -1;

         bufferId= sink->soc.drmBuffer[buffIndex].bufferId;

         numFdToSend= 1;
         offset0= offset1= offset2= 0;
         stride0= stride1= stride2= sink->soc.frameWidth;
         frameFd0= sink->soc.drmBuffer[buffIndex].fd[0];
         stride0= sink->soc.drmBuffer[buffIndex].pitch[0];
         frameFd1= sink->soc.drmBuffer[buffIndex].fd[1];
         stride1= sink->soc.drmBuffer[buffIndex].pitch[1];
         if ( frameFd1 < 0 )
         {
            offset1= sink->soc.frameWidth*sink->soc.frameHeight;
            stride1= stride0;
         }

         pixelFormat= sink->soc.frameFormatOut;

         fdToSend0= fcntl( frameFd0, F_DUPFD_CLOEXEC, 0 );
         if ( fdToSend0 < 0 )
         {
            GST_ERROR("wstSendFrameVideoClientConnectionRaw: failed to dup fd0");
            goto exit;
         }
         if ( frameFd1 >= 0 )
         {
            fdToSend1= fcntl( frameFd1, F_DUPFD_CLOEXEC, 0 );
            if ( fdToSend1 < 0 )
            {
               GST_ERROR("wstSendFrameVideoClientConnectionRaw: failed to dup fd1");
               goto exit;
            }
            ++numFdToSend;
         }
         if ( frameFd2 >= 0 )
         {
            fdToSend2= fcntl( frameFd2, F_DUPFD_CLOEXEC, 0 );
            if ( fdToSend2 < 0 )
            {
               GST_ERROR("wstSendFrameVideoClientConnectionRaw: failed to dup fd2");
               goto exit;
            }
            ++numFdToSend;
         }

         vx= sink->soc.videoX;
         vy= sink->soc.videoY;
         vw= sink->soc.videoWidth;
         vh= sink->soc.videoHeight;
         if ( needBounds(sink) )
         {
            wstGetVideoBoundsRaw( sink, &vx, &vy, &vw, &vh );
         }

         LOCK_CONN( conn );
         i= 0;
         mbody[i++]= 'V';
         mbody[i++]= 'S';
         mbody[i++]= 65;
         mbody[i++]= 'F';
         i += putU32( &mbody[i], conn->sink->soc.frameWidth );
         i += putU32( &mbody[i], conn->sink->soc.frameHeight );
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
         i += putS64( &mbody[i], sink->soc.drmBuffer[buffIndex].frameTime );

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
         FRAME("out:       send frame %d buffer %d (%d)", conn->sink->soc.frameOutCount, conn->sink->soc.drmBuffer[buffIndex].bufferId, buffIndex);

         do
         {
            sentLen= sendmsg( conn->socketFd, &msg, 0 );
         }
         while ( (sentLen < 0) && (errno == EINTR));

         conn->sink->soc.drmBuffer[buffIndex].frameNumber= conn->sink->soc.frameOutCount;

         if ( sentLen == iov[0].iov_len )
         {
            result= true;
         }
         else
         {
            FRAME("out:       failed send frame %d buffer %d (%d)", conn->sink->soc.frameOutCount, conn->sink->soc.drmBuffer[buffIndex].bufferId, buffIndex);
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
      while( !sink->soc.quitDispatchThread )
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
   LOCK(sink);
   outputFrameCount= sink->soc.frameOutCount;
   frameRate= (sink->soc.frameRate > 0.0 ? sink->soc.frameRate : 30.0);
   UNLOCK(sink);
   while( !sink->soc.quitEOSDetectionThread )
   {
      usleep( 1000000/frameRate );

      if ( !sink->soc.quitEOSDetectionThread )
      {
         LOCK(sink);
         count= sink->soc.frameOutCount;
         displayCount= sink->soc.frameDisplayCount + sink->soc.numDropped;
         videoPlaying= sink->soc.videoPlaying;
         eosEventSeen= sink->eosEventSeen;
         #ifdef USE_GENERIC_AVSYNC
         wstUpdateAVSyncCtx( sink, sink->soc.avsctx );
         #endif
         UNLOCK(sink);

         if ( sink->windowChange )
         {
            sink->windowChange= false;
            gst_westeros_sink_raw_update_video_position( sink );
         }

         if ( eosEventSeen )
         {
            GST_DEBUG("waiting for eos: frameOutCount %d displayCount %d (%d+%d)\n", count, displayCount, sink->soc.frameDisplayCount, sink->soc.numDropped);
            wstProcessMessagesVideoClientConnectionRaw( sink->soc.conn );
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

   if ( !sink->soc.quitEOSDetectionThread )
   {
      GThread *thread= sink->soc.eosDetectionThread;
      g_thread_unref( sink->soc.eosDetectionThread );
      sink->soc.eosDetectionThread= NULL;
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
      g_thread_unref( sink->soc.firstFrameThread );
      sink->soc.firstFrameThread= NULL;
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
      g_thread_unref( sink->soc.underflowThread );
      sink->soc.underflowThread= NULL;
      UNLOCK(sink);
   }

   return NULL;
}

static void wstBuildSinkCaps( GstWesterosSinkClass *klass )
{
   GstCaps *caps= 0;
   GstCaps *capsTemp= 0;
   GstPadTemplate *padTemplate= 0;

   caps= gst_caps_new_empty();
   if ( caps )
   {
      capsTemp= gst_caps_from_string(
                                       "video/x-raw, " \
                                       "format=(string) { NV12, NV21, I420, YU12 }"
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
         GST_ERROR("wstBuildSinkCaps: gst_pad_template_new failed");
      }

      gst_caps_unref( caps );
   }
   else
   {
      GST_ERROR("wstBuildSinkCaps: gst_caps_new_empty failed");
   }
}

#define DEFAULT_DRM_NAME "/dev/dri/card0"

static bool drmInit( GstWesterosSink *sink )
{
   bool result= false;
   const char *drmName;
   int rc;
   struct drm_auth auth;

   drmName= getenv("WESTEROS_SINK_DRM_NAME");
   if ( !drmName )
   {
      drmName= DEFAULT_DRM_NAME;
   }

   GST_DEBUG("drmInit");
   sink->soc.drmFd= open( drmName, O_RDWR | O_CLOEXEC );
   if ( sink->soc.drmFd < 0 )
   {
      GST_ERROR("Failed to open drm node (%s): %d", drmName, errno);
      goto exit;
   }


   memset( &auth, 0, sizeof(auth) );
   rc= ioctl( sink->soc.drmFd, DRM_IOCTL_GET_MAGIC, &auth );
   if ( rc != 0 )
   {
      GST_ERROR("drmInit: DRM_IOCTL_GET_MAGIC failed: rc %d errno %d", rc, errno);
      sink->soc.haveDrmAuthMagic= FALSE;
      sink->soc.drmAuthenticated= FALSE;
   }
   else
   {
      sink->soc.drmAuthMagic= auth.magic;
      sink->soc.haveDrmAuthMagic= TRUE;
      sink->soc.drmAuthenticated= FALSE;
      GST_WARNING("drmInit: cached DRM auth magic %u for socket authentication", (unsigned int)auth.magic);
   }
   result= true;

exit:
   return result;
}

static void drmTerm( GstWesterosSink *sink )
{
   int i;
   GST_DEBUG("drmTerm");
   if ( sink->soc.eosDetectionThread )
   {
      sink->soc.quitEOSDetectionThread= TRUE;
      g_thread_join( sink->soc.eosDetectionThread );
      sink->soc.eosDetectionThread= NULL;
   }
   if ( sink->soc.dispatchThread )
   {
      sink->soc.quitDispatchThread= TRUE;
      g_thread_join( sink->soc.dispatchThread );
      sink->soc.dispatchThread= NULL;
   }
   for( i= 0; i < WST_NUM_DRM_BUFFERS; ++i )
   {
      drmFreeBuffer( sink, i );
   }
   if ( sink->soc.drmFd >= 0 )
   {
      close( sink->soc.drmFd );
      sink->soc.drmFd= -1;
   }
   sink->soc.drmAuthenticated= FALSE;
   sink->soc.haveDrmAuthMagic= FALSE;
   sink->soc.drmAuthMagic= 0;
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
      drmBuff= &sink->soc.drmBuffer[buffIndex];

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

      rc= ioctl( sink->soc.drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_CREATE_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      memset( &mapDumb, 0, sizeof(mapDumb) );
      mapDumb.handle= createDumb.handle;

      rc= ioctl( sink->soc.drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_MAP_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      drmBuff->handle[0]= createDumb.handle;
      drmBuff->pitch[0]= createDumb.pitch;
      drmBuff->size[0]= createDumb.size;
      drmBuff->offset[0]= mapDumb.offset;

      rc= drmPrimeHandleToFD( sink->soc.drmFd, drmBuff->handle[0], DRM_CLOEXEC | DRM_RDWR, &drmBuff->fd[0] );
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
      rc= ioctl( sink->soc.drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_CREATE_DUMB failed: rc %d errno %d\n", rc, errno);
         goto exit;
      }
      memset( &mapDumb, 0, sizeof(mapDumb) );
      mapDumb.handle= createDumb.handle;
      rc= ioctl( sink->soc.drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapDumb );
      if ( rc )
      {
         GST_ERROR("DRM_IOCTL_MODE_MAP_DUMB failed: rc %d errno %d", rc, errno);
         goto exit;
      }
      drmBuff->handle[1]= createDumb.handle;
      drmBuff->pitch[1]= createDumb.pitch;
      drmBuff->size[1]= createDumb.size;
      drmBuff->offset[1]= mapDumb.offset;

      rc= drmPrimeHandleToFD( sink->soc.drmFd, drmBuff->handle[1], DRM_CLOEXEC | DRM_RDWR, &drmBuff->fd[1] );
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
   WstDrmBuffer *drmBuff= &sink->soc.drmBuffer[buffIndex];

   if ( drmBuff->localAlloc )
   {
      for( i= 0; i < WST_MAX_PLANE; ++i )
      {
         if ( drmBuff->fd[i] >= 0 )
         {
            close( drmBuff->fd[i] );
            drmBuff->fd[i]= -1;
         }
         if ( drmBuff->handle[i] )
         {
            struct drm_mode_destroy_dumb destroyDumb;

            memset( &destroyDumb, 0, sizeof(destroyDumb) );
            destroyDumb.handle= drmBuff->handle[i];
            ioctl( sink->soc.drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyDumb );
            drmBuff->handle[i]= 0;
         }
      }
   }
   else
   {
      /* For server-allocated buffers, closing the received DMA-BUF fds is sufficient. */
      for( i= 0; i < 2; ++i )
      {
         int *fd;
         if ( i == 0 )
         {
            fd= &drmBuff->fd[0];
         }
         else
         {
            fd= &drmBuff->fd[1];
         }
         if ( *fd >= 0 )
         {
            close( *fd );
            *fd= -1;
         }
      }
      if ( drmBuff->gstbuf )
      {
         gst_buffer_unref( drmBuff->gstbuf );
         drmBuff->gstbuf= 0;
      }
   }
   drmBuff->localAlloc= false;
   /* Reset dimensions so swGetSWBuffer triggers re-allocation on the next frame */
   drmBuff->width=  0;
   drmBuff->height= 0;
}

static void drmLockBuffer( GstWesterosSink *sink, int buffIndex )
{
   sink->soc.drmBuffer[buffIndex].locked= true;
   ++sink->soc.drmBuffer[buffIndex].lockCount;
}

static bool drmUnlockBuffer( GstWesterosSink *sink, int buffIndex )
{
   bool unlocked= false;
   if ( !sink->soc.drmBuffer[buffIndex].locked )
   {
      GST_ERROR("attempt to unlock buffer that is not locked: index %d", buffIndex);
   }
   if ( sink->soc.drmBuffer[buffIndex].lockCount > 0 )
   {
      if ( --sink->soc.drmBuffer[buffIndex].lockCount == 0 )
      {
         sink->soc.drmBuffer[buffIndex].locked= false;
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
      drmBuff= &sink->soc.drmBuffer[buffIndex];
      if ( drmBuff->locked )
      {
         drmBuff->locked= false;
         drmBuff->lockCount= 0;
         didUnlock= true;
      }
   }
   if ( didUnlock )
   {
      sem_post( &sink->soc.drmBuffSem );
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
      rc= sem_trywait( &sink->soc.drmBuffSem );
      if ( rc )
      {
         if ( errno == EAGAIN )
         {
            usleep( 1000 );
            wstProcessMessagesVideoClientConnectionRaw( sink->soc.conn );
            continue;
         }
      }
      break;
   }

   for( buffIndex= 0; buffIndex < WST_NUM_DRM_BUFFERS; ++buffIndex )
   {
      drmBuff= &sink->soc.drmBuffer[buffIndex];
      if ( !drmBuff->locked )
      {
         int i, imax;
         GstMemory *mem;
         #ifdef USE_GST_VIDEO
         GstVideoMeta *meta= gst_buffer_get_video_meta(buffer);
         #endif
         drmBuff->width= sink->soc.frameWidth;
         drmBuff->height= sink->soc.frameHeight;
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
               switch( sink->soc.frameFormatStream )
               {
                  case DRM_FORMAT_NV12:
                  case DRM_FORMAT_NV21:
                     sink->soc.frameFormatOut= sink->soc.frameFormatStream;
                     #ifdef USE_GST_VIDEO
                     if ( meta )
                     {
                        drmBuff->pitch[i]= meta->stride[i];
                     }
                     else
                     #endif
                     {
                        drmBuff->pitch[i]= ((sink->soc.frameWidth+63) & ~63);
                     }
                     break;
                  default:
                     GST_ERROR("Unsupported format (0x%x) for dma-buf import", sink->soc.frameFormatStream );
                     break;
               }
               GST_LOG("drmImportBuffer: buffer %p fmt %X fd %d size %d pitch %d offset %llu", buffer, sink->soc.frameFormatStream,
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
      rc= sem_trywait( &sink->soc.drmBuffSem );
      if ( rc )
      {
         if ( sink->flushStarted )
         {
            break;
         }
         if ( errno == EAGAIN )
         {
            usleep( 1000 );
            wstProcessMessagesVideoClientConnectionRaw( sink->soc.conn );
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
         sem_post( &sink->soc.drmBuffSem );
      }
      goto exit;
   }

   for( buffIndex= 0; buffIndex < WST_NUM_DRM_BUFFERS; ++buffIndex )
   {
      drmBuff= &sink->soc.drmBuffer[buffIndex];
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
   if ( !sink->soc.drmBuffer[buffIndex].locked )
   {
      int rc;
      sink->soc.drmBuffer[buffIndex].frameNumber= -1;
      FRAME("out:       release buffer %d (%d)", sink->soc.drmBuffer[buffIndex].bufferId, buffIndex);
      GST_LOG( "%lld: release: buffer %d (%d)", getCurrentTimeMillis(), sink->soc.drmBuffer[buffIndex].bufferId, buffIndex);
      if ( !sink->soc.drmBuffer[buffIndex].localAlloc )
      {
         drmFreeBuffer( sink, buffIndex );
      }
      sem_post( &sink->soc.drmBuffSem );
   }
}

static int sinkAcquireVideo( GstWesterosSink *sink )
{
   int result= 0;

   GST_DEBUG("sinkAcquireVideo: enter");

   sink->soc.haveHardware= TRUE;

   result= 1;
   GST_DEBUG("sinkAcquireVideo: exit: %d", result);

   return result;
}

static void sinkReleaseVideo( GstWesterosSink *sink )
{
   GST_DEBUG("sinkReleaseVideo: enter");

   LOCK(sink);
   sink->soc.haveHardware= FALSE;
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
      "dropped", G_TYPE_UINT64, (guint64)sink->soc.numDropped,
      "rendered", G_TYPE_UINT64, (guint64)sink->soc.frameDisplayCount, NULL);
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

