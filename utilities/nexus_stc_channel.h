#ifndef NEXUS_STC_CHANNEL_H
#define NEXUS_STC_CHANNEL_H

#ifdef NATIVE_BUILD
/*
 * Stub header for Broadcom Nexus STC (System Time Clock) channel
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

#include <stdint.h>

/* STC channel types */
typedef void* NEXUS_StcChannelHandle;

typedef struct NEXUS_StcChannelSettings {
    int dummy;
} NEXUS_StcChannelSettings;

/* STC channel functions */
static inline void NEXUS_StcChannel_GetDefaultSettings(uint32_t index, NEXUS_StcChannelSettings *settings) { 
    (void)index; 
    (void)settings; 
}

static inline NEXUS_StcChannelHandle NEXUS_StcChannel_Open(unsigned index, const NEXUS_StcChannelSettings *settings) { 
    (void)index; 
    (void)settings; 
    return (NEXUS_StcChannelHandle)1; 
}

static inline void NEXUS_StcChannel_Close(NEXUS_StcChannelHandle handle) { 
    (void)handle; 
}

#endif /* NATIVE_BUILD */

#endif /* NEXUS_STC_CHANNEL_H */