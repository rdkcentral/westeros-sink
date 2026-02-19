#ifndef GST_VIDEO_AFD_H
#define GST_VIDEO_AFD_H

#ifdef NATIVE_BUILD

/* Only define stubs if system headers haven't already provided them */
/* If gst_buffer_get_video_afd_meta is defined, the system headers were included */
#ifndef gst_buffer_get_video_afd_meta

/*
 * Stub header for GStreamer Video AFD (Active Format Description) constants
 * These constants are used for aspect ratio and letterbox/pillarbox handling
 * Only included for NATIVE_BUILD to avoid conflicts with actual GStreamer headers
 */

/* AFD (Active Format Description) enum values */
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

/* Stub functions for getting metadata from buffer */
#define gst_buffer_get_video_afd_meta(buffer) NULL
#define gst_buffer_get_video_bar_meta(buffer) NULL

#endif /* gst_buffer_get_video_afd_meta */

#endif /* NATIVE_BUILD */

#endif /* GST_VIDEO_AFD_H */