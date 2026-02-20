/*
* Copyright (C) 2016 RDK Management
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
#ifndef GST_VIDEO_AFD_H
#define GST_VIDEO_AFD_H

#ifdef NATIVE_BUILD
/*
 * Stub header for GStreamer Video AFD (Active Format Description) constants
 * These constants are used for aspect ratio and letterbox/pillarbox handling
 */

/* Only declare types if system GStreamer header hasn't already declared them */
#ifndef __GST_VIDEO_ANC_H__
typedef enum {
    GST_VIDEO_AFD_UNAVAILABLE = 0,
    GST_VIDEO_AFD_4_3_FULL_16_9_FULL = 2,
    GST_VIDEO_AFD_4_3_FULL_4_3_PILLAR = 3,
    GST_VIDEO_AFD_14_9_LETTER_14_9_PILLAR = 4,
    GST_VIDEO_AFD_4_3_FULL_14_9_CENTER = 6,
    GST_VIDEO_AFD_16_9_LETTER_16_9_FULL = 8,
    GST_VIDEO_AFD_16_9_LETTER_14_9_CENTER = 9,
    GST_VIDEO_AFD_16_9_LETTER_4_3_CENTER = 10,
    GST_VIDEO_AFD_GREATER_THAN_16_9 = 13
} GstVideoAfd;

/* AFD metadata structure for active format description */
typedef struct {
    int spec;
    int afd;
    int field;
} GstVideoAFDMeta;

/* Bar metadata structure for letterbox/pillarbox information */
typedef struct {
    int is_letterbox;
    int bar_data1;
    int bar_data2;
    int field;
} GstVideoBarMeta;

#endif /* __GST_VIDEO_ANC_H__ */

/* Stub functions for getting metadata from buffer */
#define gst_buffer_get_video_afd_meta(buffer) NULL
#define gst_buffer_get_video_bar_meta(buffer) NULL

#endif /* NATIVE_BUILD */

#endif /* GST_VIDEO_AFD_H */
