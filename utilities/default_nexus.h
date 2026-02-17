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