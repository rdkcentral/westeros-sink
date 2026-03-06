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