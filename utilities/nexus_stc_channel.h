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
