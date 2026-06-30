/*
 * Copyright (C) 2016 RDK Management
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
#ifndef __WESTEROS_SINK_RAW_H__
#define __WESTEROS_SINK_RAW_H__

typedef struct _WstVideoClientConnection WstVideoClientConnection;

/*For RAW calls From SOC*/
gboolean gst_westeros_sink_raw_setting_capabilities( GstWesterosSink *sink, GstCaps *caps );
gboolean gst_westeros_sink_raw_resource_init( GstWesterosSink *sink, gboolean *passToDefault );
gboolean gst_westeros_sink_raw_video_client_connection( GstWesterosSink *sink, gboolean *passToDefault );
void wstSetSessionInfoRaw( GstWesterosSink *sink );
void wstGetVideoBoundsRaw( GstWesterosSink *sink, int *x, int *y, int *w, int *h );
void gst_westeros_sink_raw_render( GstWesterosSink *sink, GstBuffer *buffer );
void gst_westeros_sink_raw_flush( GstWesterosSink *sink );
void gst_westeros_sink_raw_set_video_path( GstWesterosSink *sink, bool useGfxPath );
void gst_westeros_sink_raw_update_video_position( GstWesterosSink *sink );
void wstProcessMessagesVideoClientConnectionRaw( WstVideoClientConnection *conn );
void wstSetAFDInfo( GstWesterosSink *sink, GstBuffer *buffer );
void wstSinkRawStopVideo( GstWesterosSink *sink );
void wstSetTextureCropRaw( GstWesterosSink *sink, int vx, int vy, int vw, int vh );
bool wstSendFrameVideoClientConnectionRaw( WstVideoClientConnection *conn, int buffIndex );

void gst_westeros_sink_raw_term( GstWesterosSink *sink );
gboolean gst_westeros_sink_raw_paused_to_playing( GstWesterosSink *sink, gboolean *passToDefault );
gboolean gst_westeros_sink_raw_playing_to_paused( GstWesterosSink *sink, gboolean *passToDefault );
gboolean gst_westeros_sink_raw_paused_to_ready( GstWesterosSink *sink, gboolean *passToDefault );
gboolean gst_westeros_sink_raw_ready_to_null( GstWesterosSink *sink, gboolean *passToDefault );


typedef struct bufferInfo
{
   GstWesterosSink *sink;
   int buffIndex;
   int cohort;
} bufferInfo;

#endif

