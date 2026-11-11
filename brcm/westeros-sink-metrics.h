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

#ifndef __WESTEROS_SINK_METRICS_H__
#define __WESTEROS_SINK_METRICS_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* Producer side of the SoC Media Metrics GStreamer Specification v0.7 for the video decoder and
 * video output owned by the sink. Not thread safe: callers serialize access with the sink lock,
 * take the pending messages under the lock and post them after releasing it. */

typedef struct _WstVideoMetricsSample
{
   guint numDisplayed;
   guint numDrops;
   guint numDecodeErrors;
   guint numDisplayUnderflows;
   guint pts45k;
   guint fifoDepth;
   guint64 numBytesDecoded;
   gboolean inSegment;
   gboolean eos;
   gboolean captureEnabled;
} WstVideoMetricsSample;

typedef struct _WstVideoMetrics
{
   guint64 rendered;
   guint64 dropped;
   guint64 corrupted;
   gboolean haveBaseline;
   guint prevDisplayed;
   guint prevDrops;
   guint prevDecodeErrors;
   guint prevDisplayUnderflows;
   gboolean repeatActive;
   guint repeatPts45k;
   guint64 repeatStartNs;
   guint64 repeatCount;
   gboolean underflowActive;
   guint underflowPts45k;
   guint64 underflowStartNs;
   guint64 underflowBytesDecoded;
   GPtrArray *pending;
} WstVideoMetrics;

void wstVideoMetricsInit( WstVideoMetrics *metrics );
void wstVideoMetricsTerm( WstVideoMetrics *metrics );

/* Processes one decoder status sample: posts drop and decode-error occurrences and drives the
 * repeat and underflow episodes. */
void wstVideoMetricsSample( WstVideoMetrics *metrics, const WstVideoMetricsSample *sample, guint64 nowNs );

/* Starts a buffer-underflow episode from the native FIFO-empty notification. */
void wstVideoMetricsUnderflowStarted( WstVideoMetrics *metrics, guint pts45k, guint64 numBytesDecoded, guint64 nowNs );

/* Boundary resolution of the video-output repeat episode: scheduled output paused or handed off. */
void wstVideoMetricsResolveOutput( WstVideoMetrics *metrics, guint64 nowNs );

/* Boundary resolution of every episode: decoder flush/stop, resource loss, EOS or teardown. The
 * next sample re-establishes the native counter baseline of the new decoder epoch. */
void wstVideoMetricsResolveAll( WstVideoMetrics *metrics, guint64 nowNs );

/* Returns the messages waiting to be posted, or NULL. */
GPtrArray *wstVideoMetricsTakePending( WstVideoMetrics *metrics );

/* Posts and frees messages returned by wstVideoMetricsTakePending. */
void wstVideoMetricsPost( GstElement *element, GPtrArray *pending );

/* Coherent cumulative counters; corrupted is omitted when the SoC is not the final presenter. */
GstStructure *wstVideoMetricsGetStats( const WstVideoMetrics *metrics, gboolean haveFinalPresentation );

G_END_DECLS

#endif
