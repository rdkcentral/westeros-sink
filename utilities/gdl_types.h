/*
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
#ifndef GDL_TYPES_H
#define GDL_TYPES_H

#ifdef NATIVE_BUILD

/*
 * Stub header for Intel CE libgdl gdl_types.h
 * Basic GDL (Graphics Display Layer) type definitions
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

#include <stdint.h>

/* GDL result codes */
typedef enum {
    GDL_SUCCESS = 0,
    GDL_ERR_FAILED = -1,
    GDL_ERR_INVAL = -2,
    GDL_ERR_NULL_ARG = -3
} gdl_ret_t;

/* GDL pixel formats */
typedef enum {
    GDL_PF_ARGB_32 = 0,
    GDL_PF_RGB_32,
    GDL_PF_RGB_16,
    GDL_PF_YUV_420_PLANAR,
    GDL_PF_YUV_422_PACKED_YUY2
} gdl_pixel_format_t;

/* GDL boolean type */
typedef int gdl_boolean_t;

/* GDL boolean constants */
#define GDL_TRUE 1
#define GDL_FALSE 0

/* GDL size structure */
typedef struct {
    int width;
    int height;
} gdl_size_t;

/* GDL mode structure for display info */
typedef struct {
    int width;
    int height;
} gdl_mode_t;

/* GDL aspect ratio structure */
typedef struct {
    int numerator;
    int denominator;
} gdl_aspect_ratio_t;

/* GDL display structures */
typedef struct {
    unsigned int width;
    unsigned int height;
    gdl_pixel_format_t pixel_format;
    unsigned int pitch;
    void* data;
} gdl_surface_info_t;

typedef uint32_t gdl_display_id_t;
typedef uint32_t gdl_plane_id_t;
typedef uint32_t gdl_surface_id_t;

/* Display identifiers */
#define GDL_DISPLAY_ID_0 0
#define GDL_DISPLAY_ID_1 1

/* Plane identifiers */  
#define GDL_PLANE_ID_UNDEFINED -1
#define GDL_PLANE_ID_UPP_A 0
#define GDL_PLANE_ID_UPP_B 1
#define GDL_PLANE_ID_UPP_C 2
#define GDL_PLANE_ID_UPP_D 3

/* Display info structure */
typedef struct {
    unsigned int id;
    unsigned int width;
    unsigned int height;
    unsigned int refresh_rate;
    gdl_mode_t tvmode;
    unsigned int pixel_clock;
} gdl_display_info_t;

/* Rectangle structure supporting all access patterns:
   - Direct access: rect.x, rect.y
   - Offset access: rect.h_offset, rect.v_offset  
   - Nested origin: rect.origin.x, rect.origin.y
   - Dimensions: rect.width, rect.height */
typedef struct {
    int x;              /* Direct x coordinate */
    int y;              /* Direct y coordinate */
    int h_offset;       /* Horizontal offset */
    int v_offset;       /* Vertical offset */
    struct {
        int x;          /* origin.x - nested access */
        int y;          /* origin.y - nested access */
    } origin;
    int width;
    int height;
} gdl_rectangle_t;

/* GDL plane attribute constants */
#define GDL_PLANE_ALPHA_GLOBAL 0x1000
#define GDL_PLANE_VID_DST_RECT 0x2000
#define GDL_PLANE_VID_SRC_RECT 0x2001
#define GDL_PLANE_VID_MISMATCH_POLICY 0x2002

/* GDL video policy constants */
#define GDL_VID_POLICY_CONSTRAIN 0
#define GDL_VID_POLICY_SCALE 1
#define GDL_VID_POLICY_FILL 2

/* GDL port device constants */
#define GDL_PD_ID_INTTVENC 0x100
#define GDL_PD_ID_INTTVENC_COMPONENT 0x101
#define GDL_PD_ATTR_ID_CC 0x200

#endif /* NATIVE_BUILD */

#endif /* GDL_TYPES_H */