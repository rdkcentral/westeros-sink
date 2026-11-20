/*
 * Copyright (C) 2016 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Wire protocol between capture_session_feeder (server) and capture_session_renderer (client)
 * for the SoC video frame capture test.  Plain C, usable from both programs.
 *
 * Transport: AF_UNIX / SOCK_SEQPACKET.  One message per captured frame,
 * server -> client, carrying a CaptureSessionFrameMsg and nfds descriptors via
 * SCM_RIGHTS.  The client answers each frame with a CaptureSessionAckMsg once its GPU
 * reads have completed; the server then calls releaseFrame().
 */
#ifndef CAPTURE_SESSION_IPC_H
#define CAPTURE_SESSION_IPC_H

#include <stdint.h>

#define CAPTURE_SESSION_MAGIC          0x50434631u /* 'PCF1' */
#define CAPTURE_SESSION_MAX_PLANES     4
#define CAPTURE_SESSION_MAX_FDS        4
#define CAPTURE_SESSION_DEFAULT_SOCKET "/tmp/capture_session.sock"

typedef struct {
    uint32_t planeIndex;
    uint32_t fdIndex;       /* index into the SCM_RIGHTS fd array */
    uint32_t offsetBytes;
    uint32_t strideBytes;
} CaptureSessionPlane;

typedef struct {
    uint32_t magic;
    uint32_t slotCount;     /* admitted pool capacity (client cache bound) */
    uint64_t seq;           /* echoed back in the ack */
    uint64_t bufferId;      /* identity of the import description (see server) */
    uint32_t flushed;       /* dmaBufObjectsFlushed, latched across IPC failures */
    uint32_t nfds;
    uint32_t nplanes;
    uint32_t drmFormat;
    uint64_t drmModifier;
    uint32_t backingWidth, backingHeight;
    uint32_t visX, visY, visWidth, visHeight;
    int64_t  ptsNs;
    uint32_t matrix;        /* CaptureYcbcrMatrix   */
    uint32_t range;         /* CaptureSampleRange   */
    uint32_t hSiting;       /* CaptureChromaSiting  */
    uint32_t vSiting;
    CaptureSessionPlane planes[CAPTURE_SESSION_MAX_PLANES];
} CaptureSessionFrameMsg;

typedef struct {
    uint32_t magic;
    uint32_t reserved;
    uint64_t seq;
} CaptureSessionAckMsg;

#endif
