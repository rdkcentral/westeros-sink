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
#ifndef DEFAULT_NEXUS_H
#define DEFAULT_NEXUS_H

#ifdef NATIVE_BUILD
/*
 * Stub header for default_nexus.h
 * This provides basic definitions for Broadcom Nexus platform
 * for compilation without the actual Broadcom SDK
 * Only included for NATIVE_BUILD to avoid conflicts with nexus_platform.h in production builds
 */

/* Platform version macros - avoid redefinition */
#ifndef NEXUS_PLATFORM_VERSION_MAJOR
#define NEXUS_PLATFORM_VERSION_MAJOR 18
#endif
#ifndef NEXUS_PLATFORM_VERSION_MINOR
#define NEXUS_PLATFORM_VERSION_MINOR 3
#endif

/* Basic Nexus error codes */
#define NEXUS_SUCCESS 0
#define NEXUS_UNKNOWN -1

/* Forward declarations for common Nexus types */
typedef void* NEXUS_VideoDecoderHandle;
typedef void* NEXUS_VideoDecoderStartSettings;
typedef void* NEXUS_DisplayHandle;
typedef void* NEXUS_SurfaceHandle;
typedef void* NEXUS_VideoWindowHandle;

/* Basic Nexus initialization stub */
static inline int NEXUS_Platform_Init(void) { return NEXUS_SUCCESS; }
static inline void NEXUS_Platform_Uninit(void) { }

#endif /* NATIVE_BUILD */

#endif /* DEFAULT_NEXUS_H */
