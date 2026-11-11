/*
 * Copyright (C) 2026 RDK Management
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

#include "westeros-sink-metrics.h"

#include <string.h>

#define MEDIA_METRIC_MESSAGE_NAME "media-pipeline-metric"

enum
{
   MEDIA_METRIC_VIDEO_FRAME_DROP= 1,
   MEDIA_METRIC_VIDEO_DECODE_ERROR= 3,
   MEDIA_METRIC_VIDEO_FRAME_REPEAT= 5,
   MEDIA_METRIC_BUFFER_UNDERFLOW= 7
};

enum
{
   MEDIA_METRIC_OBSERVATION_OCCURRENCE= 1,
   MEDIA_METRIC_OBSERVATION_EPISODE_STARTED= 2,
   MEDIA_METRIC_OBSERVATION_EPISODE_RESOLVED= 3
};

static gint64 pts45kToNs( guint pts45k )
{
   return (gint64)gst_util_uint64_scale( pts45k, GST_SECOND, 45000 );
}

static guint counterDelta( guint current, guint *previous )
{
   /* A smaller value means the decoder restarted its counters */
   guint delta= (current >= *previous) ? current - *previous : current;
   *previous= current;
   return delta;
}

static void addMessage( WstVideoMetrics *metrics, guint metric, guint observationKind, guint pts45k,
                        guint64 count, guint64 durationNs )
{
   GstStructure *structure= gst_structure_new( MEDIA_METRIC_MESSAGE_NAME,
                                               "metric", G_TYPE_UINT, metric,
                                               "observation-kind", G_TYPE_UINT, observationKind,
                                               "pts-ns", G_TYPE_INT64, pts45kToNs(pts45k),
                                               NULL );
   if ( observationKind == MEDIA_METRIC_OBSERVATION_EPISODE_RESOLVED )
   {
      gst_structure_set( structure, "duration-ns", G_TYPE_UINT64, durationNs, NULL );
      if ( metric == MEDIA_METRIC_VIDEO_FRAME_REPEAT )
      {
         gst_structure_set( structure, "count", G_TYPE_UINT64, count, NULL );
      }
   }
   if ( !metrics->pending )
   {
      metrics->pending= g_ptr_array_new_with_free_func( (GDestroyNotify)gst_structure_free );
   }
   g_ptr_array_add( metrics->pending, structure );
}

static guint64 elapsedNs( guint64 startNs, guint64 nowNs )
{
   return (nowNs > startNs) ? nowNs - startNs : 0;
}

static void resolveRepeat( WstVideoMetrics *metrics, guint64 nowNs )
{
   if ( metrics->repeatActive )
   {
      addMessage( metrics, MEDIA_METRIC_VIDEO_FRAME_REPEAT, MEDIA_METRIC_OBSERVATION_EPISODE_RESOLVED,
                  metrics->repeatPts45k, metrics->repeatCount, elapsedNs( metrics->repeatStartNs, nowNs ) );
      metrics->repeatActive= FALSE;
   }
}

static void resolveUnderflow( WstVideoMetrics *metrics, guint64 nowNs )
{
   if ( metrics->underflowActive )
   {
      addMessage( metrics, MEDIA_METRIC_BUFFER_UNDERFLOW, MEDIA_METRIC_OBSERVATION_EPISODE_RESOLVED,
                  metrics->underflowPts45k, 0, elapsedNs( metrics->underflowStartNs, nowNs ) );
      metrics->underflowActive= FALSE;
   }
}

void wstVideoMetricsInit( WstVideoMetrics *metrics )
{
   memset( metrics, 0, sizeof(*metrics) );
}

void wstVideoMetricsTerm( WstVideoMetrics *metrics )
{
   if ( metrics->pending )
   {
      g_ptr_array_unref( metrics->pending );
   }
   memset( metrics, 0, sizeof(*metrics) );
}

void wstVideoMetricsSample( WstVideoMetrics *metrics, const WstVideoMetricsSample *sample, guint64 nowNs )
{
   guint newDrops, newErrors, newRepeats;

   if ( !metrics->haveBaseline )
   {
      metrics->prevDisplayed= sample->numDisplayed;
      metrics->prevDrops= sample->numDrops;
      metrics->prevDecodeErrors= sample->numDecodeErrors;
      metrics->prevDisplayUnderflows= sample->numDisplayUnderflows;
      metrics->haveBaseline= TRUE;
      return;
   }

   metrics->rendered += counterDelta( sample->numDisplayed, &metrics->prevDisplayed );

   newErrors= counterDelta( sample->numDecodeErrors, &metrics->prevDecodeErrors );
   metrics->corrupted += newErrors;
   for ( ; newErrors > 0; --newErrors )
   {
      addMessage( metrics, MEDIA_METRIC_VIDEO_DECODE_ERROR, MEDIA_METRIC_OBSERVATION_OCCURRENCE, sample->pts45k, 0, 0 );
   }

   /* Drops while a new decode position is established are deliberate and are not reported.
      After frame capture handoff, presentation is downstream and outside this producer. */
   newDrops= counterDelta( sample->numDrops, &metrics->prevDrops );
   if ( sample->inSegment && !sample->captureEnabled )
   {
      metrics->dropped += newDrops;
      for ( ; newDrops > 0; --newDrops )
      {
         addMessage( metrics, MEDIA_METRIC_VIDEO_FRAME_DROP, MEDIA_METRIC_OBSERVATION_OCCURRENCE, sample->pts45k, 0, 0 );
      }
   }

   newRepeats= counterDelta( sample->numDisplayUnderflows, &metrics->prevDisplayUnderflows );
   if ( sample->eos || sample->captureEnabled )
   {
      resolveRepeat( metrics, nowNs );
   }
   else if ( metrics->repeatActive )
   {
      metrics->repeatCount += newRepeats;
      if ( sample->pts45k != metrics->repeatPts45k )
      {
         resolveRepeat( metrics, nowNs );
      }
   }
   else if ( newRepeats > 0 && sample->inSegment )
   {
      metrics->repeatActive= TRUE;
      metrics->repeatPts45k= sample->pts45k;
      metrics->repeatStartNs= nowNs;
      metrics->repeatCount= newRepeats;
      addMessage( metrics, MEDIA_METRIC_VIDEO_FRAME_REPEAT, MEDIA_METRIC_OBSERVATION_EPISODE_STARTED, sample->pts45k, 0, 0 );
   }

   if ( metrics->underflowActive &&
        (sample->eos || sample->fifoDepth > 0 || sample->numBytesDecoded > metrics->underflowBytesDecoded) )
   {
      resolveUnderflow( metrics, nowNs );
   }
}

void wstVideoMetricsUnderflowStarted( WstVideoMetrics *metrics, guint pts45k, guint64 numBytesDecoded, guint64 nowNs )
{
   if ( !metrics->underflowActive )
   {
      metrics->underflowActive= TRUE;
      metrics->underflowPts45k= pts45k;
      metrics->underflowStartNs= nowNs;
      metrics->underflowBytesDecoded= numBytesDecoded;
      addMessage( metrics, MEDIA_METRIC_BUFFER_UNDERFLOW, MEDIA_METRIC_OBSERVATION_EPISODE_STARTED, pts45k, 0, 0 );
   }
}

void wstVideoMetricsResolveOutput( WstVideoMetrics *metrics, guint64 nowNs )
{
   resolveRepeat( metrics, nowNs );
}

void wstVideoMetricsResolveAll( WstVideoMetrics *metrics, guint64 nowNs )
{
   resolveRepeat( metrics, nowNs );
   resolveUnderflow( metrics, nowNs );
   metrics->haveBaseline= FALSE;
}

GPtrArray *wstVideoMetricsTakePending( WstVideoMetrics *metrics )
{
   GPtrArray *pending= metrics->pending;
   metrics->pending= NULL;
   return pending;
}

void wstVideoMetricsPost( GstElement *element, GPtrArray *pending )
{
   guint i;

   if ( !pending )
   {
      return;
   }
   for ( i= 0; i < pending->len; ++i )
   {
      GstStructure *structure= (GstStructure*)g_ptr_array_index( pending, i );
      GstMessage *msg= gst_message_new_element( GST_OBJECT_CAST(element), gst_structure_copy(structure) );
      if ( !gst_element_post_message( element, msg ) )
      {
         GST_WARNING_OBJECT( element, "unable to post %" GST_PTR_FORMAT, structure );
      }
   }
   g_ptr_array_unref( pending );
}

GstStructure *wstVideoMetricsGetStats( const WstVideoMetrics *metrics, gboolean haveFinalPresentation )
{
   GstStructure *stats= gst_structure_new( "application/x-gst-base-sink-stats",
                                           "dropped", G_TYPE_UINT64, metrics->dropped,
                                           "rendered", G_TYPE_UINT64, metrics->rendered,
                                           NULL );
   if ( haveFinalPresentation )
   {
      gst_structure_set( stats, "corrupted", G_TYPE_UINT64, metrics->corrupted, NULL );
   }
   return stats;
}
