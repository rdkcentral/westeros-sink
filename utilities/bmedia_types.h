/*
 * Stub header for bmedia_types.h (Broadcom Media Types)
 * Created for NATIVE_BUILD to enable compilation without Broadcom SDK
 */

#ifndef BMEDIA_TYPES_H
#define BMEDIA_TYPES_H

#ifdef NATIVE_BUILD

typedef enum bvideo_codec {
    bvideo_codec_unknown = 0,
    bvideo_codec_mpeg1,
    bvideo_codec_mpeg2,
    bvideo_codec_mpeg4_part2,
    bvideo_codec_h263,
    bvideo_codec_h264,
    bvideo_codec_h264_svc,
    bvideo_codec_h264_mvc,
    bvideo_codec_h265,
    bvideo_codec_vc1,
    bvideo_codec_vc1_sm,
    bvideo_codec_divx_311,
    bvideo_codec_avs,
    bvideo_codec_vp6,
    bvideo_codec_vp7,
    bvideo_codec_vp8,
    bvideo_codec_vp9,
    bvideo_codec_spark,
    bvideo_codec_mjpeg,
    bvideo_codec_rv40,
    bvideo_codec_av1,
    bvideo_codec_max
} bvideo_codec;

#endif /* NATIVE_BUILD */

#endif /* BMEDIA_TYPES_H */
