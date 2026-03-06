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
/* Stub header for Broadcom Nexus platform */
#ifndef NEXUS_PLATFORM_H
#define NEXUS_PLATFORM_H

#ifndef NATIVE_BUILD
/* Only include for production builds (BitBake) to avoid conflicts with default_nexus.h in native builds */

#include <stdint.h>
#include <stdbool.h>

// Stub definitions for Nexus types and functions
typedef struct NEXUS_PlatformSettings {
    int dummy;
} NEXUS_PlatformSettings;

typedef enum NEXUS_VideoFormat {
    NEXUS_VideoFormat_eNtsc = 0
} NEXUS_VideoFormat;

static inline void NEXUS_Platform_GetDefaultSettings(NEXUS_PlatformSettings *settings) { (void)settings; }
static inline int NEXUS_Platform_Init(const NEXUS_PlatformSettings *settings) { (void)settings; return 0; }
static inline void NEXUS_Platform_Uninit(void) {}

#endif /* ifndef NATIVE_BUILD */

#endif