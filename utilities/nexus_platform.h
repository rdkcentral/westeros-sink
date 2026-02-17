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