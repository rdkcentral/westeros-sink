#ifndef NEXUS_CORE_UTILS_H
#define NEXUS_CORE_UTILS_H

#ifdef NATIVE_BUILD
/*
 * Stub header for Broadcom Nexus core utilities
 * Only included for NATIVE_BUILD to avoid conflicts with real SDK
 */

#include <stdint.h>
#include <stdlib.h>

/* Core utility functions */
static inline void NEXUS_FlushCache(const void *ptr, size_t size) { (void)ptr; (void)size; }
static inline void* NEXUS_Memory_Allocate(size_t size) { return malloc(size); }
static inline void NEXUS_Memory_Free(void *ptr) { free(ptr); }

#endif /* NATIVE_BUILD */

#endif /* NEXUS_CORE_UTILS_H */