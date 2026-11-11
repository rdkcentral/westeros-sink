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

/* Host test of the v0.7 media metrics producer logic:
 *   gcc -I.. westeros-sink-metrics-test.c ../westeros-sink-metrics.c $(pkg-config --cflags --libs gstreamer-1.0)
 */

#include "westeros-sink-metrics.h"

#include <string.h>

static GstStructure *at( GPtrArray *pending, guint i )
{
   g_assert_nonnull( pending );
   g_assert_cmpuint( i, <, pending->len );
   return (GstStructure*)g_ptr_array_index( pending, i );
}

static void expectMetric( GstStructure *s, guint metric, guint kind )
{
   guint value= 0;
   gint64 pts= -1;
   g_assert_cmpstr( gst_structure_get_name(s), ==, "media-pipeline-metric" );
   g_assert_true( gst_structure_get_uint( s, "metric", &value ) );
   g_assert_cmpuint( value, ==, metric );
   g_assert_true( gst_structure_get_uint( s, "observation-kind", &value ) );
   g_assert_cmpuint( value, ==, kind );
   g_assert_true( gst_structure_get_int64( s, "pts-ns", &pts ) );
   g_assert_cmpint( pts, >=, 0 );
}

static WstVideoMetricsSample baseSample( void )
{
   WstVideoMetricsSample s;
   memset( &s, 0, sizeof(s) );
   s.inSegment= TRUE;
   s.pts45k= 45000;
   return s;
}

static void testOccurrencesAndCounters( void )
{
   WstVideoMetrics m;
   WstVideoMetricsSample s= baseSample();
   GPtrArray *p;
   GstStructure *stats;
   guint64 v= 0;

   wstVideoMetricsInit( &m );
   wstVideoMetricsSample( &m, &s, 0 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );

   s.numDisplayed= 30; s.numDrops= 2; s.numDecodeErrors= 1;
   wstVideoMetricsSample( &m, &s, 1 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 3 );
   expectMetric( at(p, 0), 3, 1 );
   expectMetric( at(p, 1), 1, 1 );
   expectMetric( at(p, 2), 1, 1 );
   g_assert_false( gst_structure_has_field( at(p, 1), "duration-ns" ) );
   g_ptr_array_unref( p );

   stats= wstVideoMetricsGetStats( &m, TRUE );
   g_assert_true( gst_structure_get_uint64( stats, "rendered", &v ) ); g_assert_cmpuint( v, ==, 30 );
   g_assert_true( gst_structure_get_uint64( stats, "dropped", &v ) ); g_assert_cmpuint( v, ==, 2 );
   g_assert_true( gst_structure_get_uint64( stats, "corrupted", &v ) ); g_assert_cmpuint( v, ==, 1 );
   gst_structure_free( stats );

   /* Flush: counters stay monotonic and the restarted decoder counters re-baseline */
   wstVideoMetricsResolveAll( &m, 2 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );
   s.numDisplayed= 0; s.numDrops= 5; s.numDecodeErrors= 0; s.inSegment= FALSE;
   wstVideoMetricsSample( &m, &s, 3 );
   s.numDrops= 9;
   wstVideoMetricsSample( &m, &s, 4 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );
   s.inSegment= TRUE; s.numDrops= 10; s.numDisplayed= 5;
   wstVideoMetricsSample( &m, &s, 5 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   g_ptr_array_unref( p );
   stats= wstVideoMetricsGetStats( &m, FALSE );
   g_assert_true( gst_structure_get_uint64( stats, "rendered", &v ) ); g_assert_cmpuint( v, ==, 35 );
   g_assert_true( gst_structure_get_uint64( stats, "dropped", &v ) ); g_assert_cmpuint( v, ==, 3 );
   g_assert_false( gst_structure_has_field( stats, "corrupted" ) );
   gst_structure_free( stats );
   wstVideoMetricsTerm( &m );
}

static void testRepeatEpisode( void )
{
   WstVideoMetrics m;
   WstVideoMetricsSample s= baseSample();
   GPtrArray *p;
   guint64 v= 0;
   gint64 pts= 0;

   wstVideoMetricsInit( &m );
   wstVideoMetricsSample( &m, &s, 0 );
   s.numDisplayUnderflows= 1;
   wstVideoMetricsSample( &m, &s, 1000 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 5, 2 );
   g_ptr_array_unref( p );

   s.numDisplayUnderflows= 3;
   wstVideoMetricsSample( &m, &s, 2000 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );

   s.pts45k= 46500;
   wstVideoMetricsSample( &m, &s, 41000 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 5, 3 );
   g_assert_true( gst_structure_get_uint64( at(p, 0), "count", &v ) ); g_assert_cmpuint( v, ==, 3 );
   g_assert_true( gst_structure_get_uint64( at(p, 0), "duration-ns", &v ) ); g_assert_cmpuint( v, ==, 40000 );
   g_assert_true( gst_structure_get_int64( at(p, 0), "pts-ns", &pts ) ); g_assert_cmpint( pts, ==, GST_SECOND );
   g_ptr_array_unref( p );

   /* Pause resolves the output episode at the boundary */
   s.numDisplayUnderflows= 4;
   wstVideoMetricsSample( &m, &s, 50000 );
   g_ptr_array_unref( wstVideoMetricsTakePending( &m ) );
   wstVideoMetricsResolveOutput( &m, 60000 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 5, 3 );
   g_assert_true( gst_structure_get_uint64( at(p, 0), "duration-ns", &v ) ); g_assert_cmpuint( v, ==, 10000 );
   g_ptr_array_unref( p );

   /* No repeat episode after EOS */
   s.eos= TRUE; s.numDisplayUnderflows= 6;
   wstVideoMetricsSample( &m, &s, 70000 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );
   wstVideoMetricsTerm( &m );
}

static void testUnderflowEpisode( void )
{
   WstVideoMetrics m;
   WstVideoMetricsSample s= baseSample();
   GPtrArray *p;
   guint64 v= 0;

   wstVideoMetricsInit( &m );
   wstVideoMetricsSample( &m, &s, 0 );
   wstVideoMetricsUnderflowStarted( &m, 45000, 100, 10 );
   wstVideoMetricsUnderflowStarted( &m, 45000, 100, 20 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 7, 2 );
   g_ptr_array_unref( p );

   /* Output pause keeps the decoder underflow */
   wstVideoMetricsResolveOutput( &m, 30 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );

   s.numBytesDecoded= 100;
   wstVideoMetricsSample( &m, &s, 40 );
   g_assert_null( wstVideoMetricsTakePending( &m ) );
   s.fifoDepth= 4096;
   wstVideoMetricsSample( &m, &s, 110 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 7, 3 );
   g_assert_true( gst_structure_get_uint64( at(p, 0), "duration-ns", &v ) ); g_assert_cmpuint( v, ==, 100 );
   g_assert_false( gst_structure_has_field( at(p, 0), "count" ) );
   g_ptr_array_unref( p );

   /* Teardown resolves an active episode */
   wstVideoMetricsUnderflowStarted( &m, 45000, 200, 200 );
   g_ptr_array_unref( wstVideoMetricsTakePending( &m ) );
   wstVideoMetricsResolveAll( &m, 250 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 7, 3 );
   g_ptr_array_unref( p );
   wstVideoMetricsTerm( &m );
}

static void testCaptureHandoff( void )
{
   WstVideoMetrics m;
   WstVideoMetricsSample s= baseSample();
   GPtrArray *p;

   wstVideoMetricsInit( &m );
   wstVideoMetricsSample( &m, &s, 0 );
   s.captureEnabled= TRUE; s.numDrops= 3; s.numDisplayUnderflows= 2; s.numDecodeErrors= 1;
   wstVideoMetricsSample( &m, &s, 1 );
   p= wstVideoMetricsTakePending( &m );
   g_assert_cmpuint( p->len, ==, 1 );
   expectMetric( at(p, 0), 3, 1 );
   g_ptr_array_unref( p );
   wstVideoMetricsTerm( &m );
}

static void testPostOnBus( void )
{
   WstVideoMetrics m;
   GstElement *element= gst_pipeline_new( "metrics" );
   GstBus *bus= gst_element_get_bus( element );
   GstMessage *msg;

   wstVideoMetricsInit( &m );
   wstVideoMetricsUnderflowStarted( &m, 0, 0, 0 );
   wstVideoMetricsPost( element, wstVideoMetricsTakePending( &m ) );
   msg= gst_bus_pop_filtered( bus, GST_MESSAGE_ELEMENT );
   g_assert_nonnull( msg );
   g_assert_true( GST_MESSAGE_SRC(msg) == GST_OBJECT_CAST(element) );
   expectMetric( (GstStructure*)gst_message_get_structure(msg), 7, 2 );
   gst_message_unref( msg );
   wstVideoMetricsPost( element, NULL );
   gst_object_unref( bus );
   gst_object_unref( element );
   wstVideoMetricsTerm( &m );
}

int main( int argc, char **argv )
{
   gst_init( &argc, &argv );
   g_test_init( &argc, &argv, NULL );
   g_test_add_func( "/metrics/occurrences-and-counters", testOccurrencesAndCounters );
   g_test_add_func( "/metrics/repeat-episode", testRepeatEpisode );
   g_test_add_func( "/metrics/underflow-episode", testUnderflowEpisode );
   g_test_add_func( "/metrics/capture-handoff", testCaptureHandoff );
   g_test_add_func( "/metrics/post-on-bus", testPostOnBus );
   return g_test_run();
}
