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
 * Broadcom ICaptureSession (SPECIFICATION-soc-video-frame-capture.md).
 *
 * Backing strategy: zero-copy.  The westeros sink runs its NEXUS_SimpleVideoDecoder in
 * buffer mode with display disabled; BufferCapture creates the decoder's output
 * NEXUS_VideoBuffers and queues them: one shared buffer per admitted slot, plus a few
 * local buffers, as many as the decoder requires for the codec, that only keep the
 * decoder running while every shared buffer is held.
 * A shared buffer is wrapped once in a NEXUS_VideoBufferContainer and exported once as
 * dma-buf fds with NEXUS_Platform_CreateMemoryBlockDescriptor_driver(); the container and
 * the fds are cached with the buffer and closed when the buffer is destroyed.  Each
 * decoded picture becomes the current selection by buffer id; acquireCurrentFrame()
 * hands out duplicates of the cached fds, and releaseFrame() (or being superseded) gives
 * the buffer back to the decoder.
 */
#include "ICaptureSession.h"
#include "capture-broadcom-ext.h"

#include <unistd.h>

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <new>
#include <thread>

#include <drm_brcm/drm_fourcc.h>

#include "nexus_base_os.h"
#include "nexus_platform.h"
#include "nexus_simple_stc_channel.h"
#include "nexus_simple_video_decoder.h"
#include "nexus_video_buffer.h"

#include <gst/gst.h>

using namespace rdkcapture;

/* Own debug category, controlled separately from the sink with
 * GST_DEBUG=capturesession:N.  This library is loaded on its own (the feeder
 * dlopens it), so the category lives here rather than in the plugin. */
GST_DEBUG_CATEGORY_STATIC(gst_capture_session_debug);
#define GST_CAT_DEFAULT gst_capture_session_debug

static void initDebugCategory()
{
    static std::once_flag once;
    std::call_once(once, []() {
        GST_DEBUG_CATEGORY_INIT(gst_capture_session_debug, "capturesession", 0, "capture session");
    });
}

namespace
{

constexpr uint32_t kMaxWidth = 1920;
constexpr uint32_t kMaxHeight = 1080;
constexpr uint32_t kMinSlots = 12;
constexpr uint32_t kMaxSlots = 20;
constexpr uint32_t kExtraLocalBuffers = 3; 

/* PTS and STC are in 45kHz ticks. */
constexpr uint32_t kPtsTicksPerMs = 45;
/* A frame whose pts is at most this far ahead of the STC is due (about 16.7ms, as in
 * TextureData::AcquireVideoFrame). */
constexpr int32_t kMatureTicks = 750;
/* How far the decoder is allowed to run ahead of the STC on local buffers alone (50ms). */
constexpr int32_t DECODING_DEPTH_LOCAL_BUFFERS = 100 * kPtsTicksPerMs;
/* After the health check found no idle local buffer to queue, it waits this long before
 * trying again. */
constexpr unsigned kLocalStarvedRetryMs = 1000;

bool sandModifier(unsigned stripedWidth, unsigned scbMapVersion, uint64_t &modifier)
{
    switch (stripedWidth)
    {
    case 128:
        if (scbMapVersion == 5)
        {
            modifier = DRM_FORMAT_MOD_BROADCOM_SAND_128_MAP5;
            return true;
        }
        return false;
    case 256:
        switch (scbMapVersion)
        {
        case 5: modifier = DRM_FORMAT_MOD_BROADCOM_SAND_256_MAP5; return true;
        case 8: modifier = DRM_FORMAT_MOD_BROADCOM_SAND_256_MAP8; return true;
        case 0xB1: modifier = DRM_FORMAT_MOD_BROADCOM_SAND_256_MAPB1; return true;
        case 0xB2: modifier = DRM_FORMAT_MOD_BROADCOM_SAND_256_MAPB2; return true;
        default: return false;
        }
    default:
        return false;
    }
}

bool platformSandModifier(uint64_t &modifier)
{
    NEXUS_PlatformGeneralStatus ps;
    if (NEXUS_Platform_GetGeneralStatus(&ps) != NEXUS_SUCCESS)
    {
        return false;
    }
    return sandModifier(ps.memc[0].stripedWidth, ps.memc[0].scbMapVersion, modifier);
}

bool chromaFourcc(NEXUS_PixelFormat chromaFormat, uint32_t &fourcc)
{
    switch (chromaFormat)
    {
    case NEXUS_PixelFormat_eCr8_Cb8: fourcc = DRM_FORMAT_NV12; return true;
    case NEXUS_PixelFormat_eCb8_Cr8: fourcc = DRM_FORMAT_NV21; return true;
    case NEXUS_PixelFormat_eCb10_Cr10: fourcc = DRM_FORMAT_P030; return true;
    default: return false; /* e.g. Cr10_Cb10 has no DRM fourcc */
    }
}

/*
 * Drives a NEXUS_SimpleVideoDecoder running in buffer mode with display disabled.
 * The decoder only ever writes into buffers we create and queue, so decoded
 * pictures never reach the HW display.
 *
 * Two kinds of buffer (Entry::shared) exist:
 *  - shared: backs a capture slot.  It owns a NEXUS_VideoBufferContainer, created on the
 *    first decoded picture, and the dma-buf fds exported from it, exported once and
 *    cached.  Decoded pictures are offered to the frame callback by buffer id and the
 *    buffer is held (kHeld) until the session gives it back with releaseBuffer().
 *    The container and the fds live and die with the NEXUS_VideoBuffer.
 *  - local: only keeps the decoder running when no shared buffer is available to it
 *    (all held by the session / MW).  Never offered, never exported; it only travels
 *    between the decoder and idle.  It is never put in m_pending / m_pendingDeferred (those
 *    hold shared buffers only): its flags are updated and queueLocalThrottling() queues it,
 *    paced by the STC, when the health check that runs on every pass of the capture thread
 *    (about every 16ms) finds no shared buffer available to the decoder and the decoded
 *    pictures falling behind the STC.  While shared buffers are released in time it never
 *    queues one.
 *
 * A buffer returns to the decoder only once nothing uses it any more: it is not
 * queued to the decoder (kDecoderWrite), the decoder no longer references it for
 * prediction (kDecoderRead), the session does not hold it (kHeld) and it is not waiting for
 * a free session slot in m_offering (kOffering).
 */
/* What the session did with an offered picture. */
enum class OfferResult
{
    Accepted, /* stored in a slot: the session keeps the buffer until it is released */
    Declined, /* not wanted (not connected, unsupported, ...): the buffer can be recycled */
    Full      /* no free slot right now (e.g. the slots still hold the old resolution's frames
               * during a resolution change): offer it again later */
};

class BufferCapture
{
public:
    typedef std::function<OfferResult(uint64_t, const NEXUS_VideoBufferStatus &)> FrameFn;

    /* Called from healthCheck() on every pass of the capture thread, with m_lock held, so the
     * session can free slots nobody acquires.  It may call releaseBuffer() (m_lock is recursive). */
    typedef std::function<void()> HealthFn;

    BufferCapture(FrameFn onFrame, HealthFn onHealthCheck)
        : m_onFrame(std::move(onFrame)), m_onHealthCheck(std::move(onHealthCheck)) {}
    ~BufferCapture() { stop(); }

    /* Number of shared buffers (the admitted slot count) allocated for the next
     * requirements event. */
    void setSharedCount(unsigned count)
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        m_sharedCount = count;
        GST_LOG("%s: shared count %u", __FUNCTION__, count);
    }

    /* The STC the local buffer throttling is paced by; NULL paces by the timer alone. */
    void setStcChannel(NEXUS_SimpleStcChannelHandle stcChannel)
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        m_stcChannel = stcChannel;
        GST_DEBUG("%s: stc channel %p", __FUNCTION__, (void *)stcChannel);
    }

    /* A flush is under way: the next decoded picture re-seeds the STC.  Also done when the
     * decoder reports eFlushed, which orders it after pictures decoded before the flush. */
    void flush()
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        m_firstFrameReceived = false;
        dropOffering(); /* pictures decoded before the flush are not wanted any more */
    }

    /* Install the output event on the decoder.  Call before NEXUS_SimpleVideoDecoder_Start()
     * so no output event of the decoder is missed; start() calls it if it was not. */
    bool prepare(NEXUS_SimpleVideoDecoderHandle decoder)
    {
        if (m_event)
        {
            return true;
        }
        GST_DEBUG("%s: decoder %p", __FUNCTION__, (void *)decoder);
        m_event = NEXUS_Event_Create();
        if (!m_event)
        {
            GST_ERROR("%s: NEXUS_Event_Create failed", __FUNCTION__);
            return false;
        }

        NEXUS_VideoDecoderExtendedSettings ext;
        NEXUS_SimpleVideoDecoder_GetExtendedSettings(decoder, &ext);
        ext.dataReadyCallback.callback = NULL;
        ext.dataReadyCallback.context = NULL;
        ext.dataReadyCallback.event = m_event;
        if (NEXUS_SimpleVideoDecoder_SetExtendedSettings(decoder, &ext) != NEXUS_SUCCESS)
        {
            GST_ERROR("%s: NEXUS_SimpleVideoDecoder_SetExtendedSettings failed", __FUNCTION__);
            NEXUS_Event_Destroy(m_event);
            m_event = nullptr;
            return false;
        }
        m_prepDecoder = decoder;
        return true;
    }

    bool start(NEXUS_SimpleVideoDecoderHandle decoder)
    {
        if (m_running)
        {
            GST_WARNING("%s: already running", __FUNCTION__);
            return true;
        }
        GST_DEBUG("%s: decoder %p", __FUNCTION__, (void *)decoder);
        if (!prepare(decoder))
        {
            return false;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);
            m_decoder = decoder;
            m_cur = Requirements();
            m_haveRequirements = false;
            m_receivedNewSized = true;
            m_havePts = false;
            m_firstFrameReceived = false;
            m_avgPtsTicks = 0;
            m_nextLocalCheckValid = false;
            m_offering.clear();
        }

        NEXUS_VideoDecoderSettings videoSettings;
        NEXUS_SimpleVideoDecoder_GetSettings(m_decoder, &videoSettings);
        m_videoDecoderSettings = videoSettings;
        videoSettings.maxWidth  = 1920;
        videoSettings.maxHeight = 1080;
        NEXUS_SimpleVideoDecoder_SetSettings(m_decoder, &videoSettings);

        m_quit = false;
        m_running = true;
        m_thread = std::thread(&BufferCapture::run, this);
        GST_DEBUG("%s: capture thread started", __FUNCTION__);
        return true;
    }

    void stop()
    {
        if (!m_running)
        {
            if (m_event)
            {
                /* prepared, never started */
                NEXUS_VideoDecoderExtendedSettings ext;
                NEXUS_SimpleVideoDecoder_GetExtendedSettings(m_prepDecoder, &ext);
                ext.dataReadyCallback.event = nullptr;
                NEXUS_SimpleVideoDecoder_SetExtendedSettings(m_prepDecoder, &ext);
                NEXUS_Event_Destroy(m_event);
                m_event = nullptr;
            }
            return;
        }
        GST_DEBUG("%s: stopping capture thread", __FUNCTION__);
        m_quit = true;
        NEXUS_Event_Signal(m_event);
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        m_running = false;

        NEXUS_SimpleVideoDecoderHandle decoder;
        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);
            decoder = m_decoder;
            m_decoder = nullptr;
        }
        if (decoder)
        {
            NEXUS_VideoDecoderExtendedSettings ext;
            NEXUS_SimpleVideoDecoder_GetExtendedSettings(decoder, &ext);
            ext.dataReadyCallback.event = nullptr;
            NEXUS_SimpleVideoDecoder_SetExtendedSettings(decoder, &ext);

            /* EXPERIMENT: do not restore the saved settings onto the still-running decoder
            NEXUS_SimpleVideoDecoder_SetSettings(decoder, &m_videoDecoderSettings); */
        }
        NEXUS_Event_Destroy(m_event);
        m_event = nullptr;
        GST_DEBUG("%s: stopped", __FUNCTION__);
    }

    /* Give back a buffer previously accepted by the frame callback.  Callable from any
     * thread; a no-op once stop() has destroyed the buffers. */
    void releaseBuffer(uint64_t id)
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        GST_TRACE("%s: buffer %llu(0x%llx)", __FUNCTION__, (unsigned long long)id, (unsigned long long)id);
        releaseFlags(id, kHeld, false);
    }

    /* Fill "frame" (format, modifier, dma-buf objects, planes, fds) for a held shared
     * buffer.  The buffer is exported on first use and the fds are cached; the frame
     * receives duplicates, which the MW closes.  On failure "frame" is left empty. */
    CaptureStatus exportFrame(uint64_t id, CapturedFrame &frame)
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        EntryMap::iterator it = m_entries.find(id);
        if (it == m_entries.end() || !it->second.shared || !(it->second.flags & kHeld) ||
            !it->second.container)
        {
            GST_WARNING("%s: buffer %llu is not a held shared buffer", __FUNCTION__, (unsigned long long)id);
            return CaptureStatus::NO_FRAME;
        }
        Entry &e = it->second;
        /* Checked for every frame: the layout depends on the picture the buffer holds now. */
        CaptureStatus status = exportEntry(e);
        if (status != CaptureStatus::OK)
        {
            GST_ERROR("%s: export of buffer %llu failed, status %d", __FUNCTION__, (unsigned long long)id, (int)status);
            return status;
        }
        return fillFrame(e.exp.objectId, e.exp, frame);
    }

private:
    /* BXVD's hardware input queue has a fixed depth of 20 entries shared by every
     * output buffer queued to the decoder; stay under it so a resolution change
     * (old- and new-sized buffer sets queued together) can not overrun it. */
    static constexpr unsigned kMaxQueuedBuffers = 18;

    enum : unsigned
    {
        kDecoderWrite = 1, /* queued to the decoder to be filled */
        kDecoderRead = 2,  /* decoded, but the decoder still references it */
        kHeld = 4,         /* offered to and kept by the capture session */
        kOffering = 8      /* decoded picture waiting in m_offering for a free session slot */
    };

    struct Requirements
    {
        unsigned width{0};
        unsigned height{0};
        unsigned colorDepth{0};
        NEXUS_VideoCodec codec{NEXUS_VideoCodec_eUnknown};
    };

    /* Cached dma-buf export of a shared buffer. */
    /* What the plane descriptors of a buffer are derived from.  The decoder reports it per
     * decoded picture: the stripe NPX follows the width of the picture, not of the buffer, so
     * a small picture decoded into a larger buffer (resolution change) has a different layout
     * than a picture that fills it. */
    struct Layout
    {
        unsigned lumaStripeHeight{0};
        unsigned chromaStripeHeight{0};
        unsigned lumaStripeNPX{0};
        unsigned chromaStripeNPX{0};
        unsigned lumaBlockOffset{0};
        unsigned chromaBlockOffset{0};

        bool operator==(const Layout &o) const
        {
            return lumaStripeHeight == o.lumaStripeHeight && chromaStripeHeight == o.chromaStripeHeight &&
                   lumaStripeNPX == o.lumaStripeNPX && chromaStripeNPX == o.chromaStripeNPX &&
                   lumaBlockOffset == o.lumaBlockOffset && chromaBlockOffset == o.chromaBlockOffset;
        }
        bool operator!=(const Layout &o) const { return !(*this == o); }
    };

    struct Export
    {
        bool done{false};
        int fd{-1}; /* the one memory block holding both planes */
        uint64_t objectId{0}; /* dmaBufObjectId of the frames: changes with the layout */
        Layout layout;        /* what the descriptor below was built from */
        uint32_t fourcc{0};
        uint64_t modifier{0};
        uint64_t lumaSize{0};
        uint64_t chromaSize{0};
        uint64_t lumaOffset{0};
        uint64_t chromaOffset{0};
        uint32_t lumaPitch{0};
        uint32_t chromaPitch{0};
    };

    struct Entry
    {
        NEXUS_VideoBufferHandle buffer{nullptr};
        uint64_t id{0}; /* NEXUS_VideoBufferStatus::id: the dmaBufObjectId of its frames until their layout changes (exp.objectId) */
        bool shared{false};
        unsigned width{0};
        unsigned height{0};
        unsigned colorDepth{0};
        NEXUS_VideoCodec codec{NEXUS_VideoCodec_eUnknown};
        unsigned flags{0};
        bool hasPts{false}; /* pts of the picture last decoded into it */
        uint32_t pts{0};
        NEXUS_VideoBufferContainerHandle container{nullptr}; /* shared only */
        Export exp;                                          /* shared only */
    };

    typedef std::map<uint64_t, Entry> EntryMap;

    bool matchesCurrent(unsigned w, unsigned h, unsigned depth, NEXUS_VideoCodec codec) const
    {
        return w == m_cur.width && h == m_cur.height && depth == m_cur.colorDepth && codec == m_cur.codec;
    }

    /* The fds close along with the buffer they were exported from. */
    static void destroyEntry(Entry &e)
    {
        if (e.exp.fd >= 0)
        {
            close(e.exp.fd);
        }
        if (e.container)
        {
            NEXUS_VideoBufferContainer_Destroy(e.container);
        }
        NEXUS_VideoBuffer_Destroy(e.buffer);
        e = Entry();
    }

    void run()
    {
        GST_DEBUG("%s: enter", __FUNCTION__);
        while (!m_quit)
        {
            NEXUS_VideoDecoderOutputEvent ev;
            unsigned num = 0;
            while (!m_quit &&
                   NEXUS_SimpleVideoDecoder_GetOutputEvents(m_decoder, &ev, 1, &num) == NEXUS_SUCCESS &&
                   num == 1)
            {
                /* GetOutputEvents only copies the event; buffers it releases can not be
                 * queued again until it has been consumed. */
                NEXUS_SimpleVideoDecoder_ConsumeOutputEvents(m_decoder, 1);
                std::lock_guard<std::recursive_mutex> lock(m_lock);
                handleEvent(ev);
            }
            {
                std::lock_guard<std::recursive_mutex> lock(m_lock);
                healthCheck();
            }
            NEXUS_Event_Wait(m_event, 16);
        }

        std::lock_guard<std::recursive_mutex> lock(m_lock);
        for (EntryMap::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
        {
            destroyEntry(it->second);
        }
        m_entries.clear();
        m_pending.clear();
        m_pendingDeferred.clear();
        m_offering.clear();
        GST_DEBUG("%s: exit", __FUNCTION__);
    }

    /* Name of an output event type, in the order handleEvent() switches on them. */
    static const char *eventTypeName(NEXUS_VideoDecoderOutputEventType type)
    {
        switch (type)
        {
        case NEXUS_VideoDecoderOutputEventType_eRequirementsChanged:
            return "RequirementsChanged";
        case NEXUS_VideoDecoderOutputEventType_eDecodeSuccess:
            return "DecodeSuccess";
        case NEXUS_VideoDecoderOutputEventType_eDecodeAndRelease:
            return "DecodeAndRelease";
        case NEXUS_VideoDecoderOutputEventType_eReferenceReleased:
            return "ReferenceReleased";
        case NEXUS_VideoDecoderOutputEventType_eFlushed:
            return "Flushed";
        case NEXUS_VideoDecoderOutputEventType_eBufferIncompatible:
            return "BufferIncompatible";
        default:
            return "NotInterested";
        }
    }

    void handleEvent(const NEXUS_VideoDecoderOutputEvent &ev)
    {
        GST_TRACE("%s: event type %s (%d) buffer %p", __FUNCTION__, eventTypeName(ev.type), (int)ev.type, (void *)ev.buffer);
        switch (ev.type)
        {
        case NEXUS_VideoDecoderOutputEventType_eRequirementsChanged:
            GST_LOG("%s: %ux%u depth %u codec %u\n", __FUNCTION__,
                    ev.requirements.width, ev.requirements.height, ev.requirements.colorDepth, (unsigned)ev.requirements.codec);
            onRequirements(ev);
            break;
        case NEXUS_VideoDecoderOutputEventType_eDecodeSuccess:
        case NEXUS_VideoDecoderOutputEventType_eDecodeAndRelease:
            onDecoded(ev);
            break;
        case NEXUS_VideoDecoderOutputEventType_eReferenceReleased:
        case NEXUS_VideoDecoderOutputEventType_eFlushed:
            if (ev.type == NEXUS_VideoDecoderOutputEventType_eFlushed)
            {
                GST_DEBUG("%s: decoder flushed", __FUNCTION__);
                m_havePts = false; /* the pts of what follows is unrelated to the latest one */
                m_firstFrameReceived = false; /* the next picture re-seeds the STC */
            }
            if (ev.buffer)
            {
                releaseFlags(bufferId(ev), kDecoderWrite | kDecoderRead, false);
            }
            break;
        case NEXUS_VideoDecoderOutputEventType_eBufferIncompatible:
            GST_WARNING("%s: buffer %p incompatible", __FUNCTION__, (void *)ev.buffer);
            if (ev.buffer)
            {
                releaseFlags(bufferId(ev), kDecoderWrite | kDecoderRead, true);
            }
            break;
        default:
            break;
        }
    }

    static uint64_t bufferId(const NEXUS_VideoDecoderOutputEvent &ev)
    {
        NEXUS_VideoBufferStatus status;
        NEXUS_VideoBuffer_GetStatus(ev.buffer, &status);
        return status.id;
    }

    enum class DestroyMode
    {
        Idle,        /* every idle buffer */
        IdleOldSized /* idle buffers that are not of the current size */
    };

    /* Destroy buffers nothing uses, per mode.  A destroyed buffer is taken out of m_pending
     * and m_pendingDeferred too.  What is left in those lists must agree with m_entries
     * (see checkQueuedLocked). */
    void destroyEntries(DestroyMode mode)
    {
        for (EntryMap::iterator it = m_entries.begin(); it != m_entries.end();)
        {
            Entry &e = it->second;
            if (e.flags == 0 &&
                (mode == DestroyMode::Idle || !matchesCurrent(e.width, e.height, e.colorDepth, e.codec)))
            {
                const uint64_t id = it->first;
                GST_DEBUG("%s: destroying idle buffer %llu (%s)", __FUNCTION__, (unsigned long long)id, e.shared ? "shared" : "local");
                destroyEntry(e);
                m_entries.erase(it++);
                m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), id), m_pending.end());
                m_pendingDeferred.erase(std::remove(m_pendingDeferred.begin(), m_pendingDeferred.end(), id), m_pendingDeferred.end());
            }
            else
            {
                ++it;
            }
        }
        /* paranoia */
        checkQueuedLocked(mode);
    }

    /* After destroying every idle buffer (Idle) no buffer waiting in m_pending or
     * m_pendingDeferred may remain in m_entries, as waiting buffers are idle.  After destroying
     * only the old-sized ones (IdleOldSized), every waiting buffer must still be in m_entries. */
    void checkQueuedLocked(DestroyMode mode) const
    {
        const bool onlyOldSized = (mode == DestroyMode::IdleOldSized);
        const std::deque<uint64_t> *lists[2] = {&m_pending, &m_pendingDeferred};
        const char *names[2] = {"m_pending", "m_pendingDeferred"};
        for (int l = 0; l < 2; ++l)
        {
            for (std::deque<uint64_t>::const_iterator it = lists[l]->begin(); it != lists[l]->end(); ++it)
            {
                const bool found = m_entries.find(*it) != m_entries.end();
                if (onlyOldSized && !found)
                {
                    fprintf(stderr, "icapturesession: buffer %llu in %s is not found in m_entries\n",
                            (unsigned long long)*it, names[l]);
                    assert(found);
                }
                else if (!onlyOldSized && found)
                {
                    fprintf(stderr, "icapturesession: buffer %llu in %s is still in m_entries after destroying "
                                    "all idle buffers\n", (unsigned long long)*it, names[l]);
                    assert(!found);
                }
            }
        }
    }

    bool addEntry(const NEXUS_VideoBufferCreateSettings &settings, bool shared)
    {
        NEXUS_VideoBufferHandle buffer = NEXUS_VideoBuffer_Create(&settings);
        if (!buffer)
        {
            GST_ERROR("%s: NEXUS_VideoBuffer_Create failed (%s)", __FUNCTION__, shared ? "shared" : "local");
            return false;
        }
        NEXUS_VideoBufferStatus status;
        NEXUS_VideoBuffer_GetStatus(buffer, &status);
        GST_DEBUG("%s: added %s buffer %llu", __FUNCTION__, shared ? "shared" : "local", (unsigned long long)status.id);
        Entry e;
        e.buffer = buffer;
        e.id = status.id;
        e.shared = shared;
        e.width = m_cur.width;
        e.height = m_cur.height;
        e.colorDepth = m_cur.colorDepth;
        e.codec = m_cur.codec;
        m_entries[status.id] = e;
        if (shared)
        {
            m_pending.push_back(status.id);
        }
        return true;
    }

    void onRequirements(const NEXUS_VideoDecoderOutputEvent &ev)
    {
        if (m_haveRequirements &&
            matchesCurrent(ev.requirements.width, ev.requirements.height, ev.requirements.colorDepth,
                           ev.requirements.codec))
        {
            return;
        }

        const bool resize = m_haveRequirements;
        if (resize)
        {
            GST_WARNING("%s: requirements changed %ux%u depth %u -> %ux%u depth %u", __FUNCTION__,
                      m_cur.width, m_cur.height, m_cur.colorDepth,
                      ev.requirements.width, ev.requirements.height, ev.requirements.colorDepth);
        }
        m_haveRequirements = true;
        m_cur.width = ev.requirements.width;
        m_cur.height = ev.requirements.height;
        m_cur.colorDepth = ev.requirements.colorDepth;
        m_cur.codec = ev.requirements.codec;

        if (resize)
        {
            /* The decoder may still have old-sized input to decode, so old-sized buffers keep
             * being recycled until the first new-sized picture arrives (see onDecoded). Only
             * buffers nothing uses can go now. */
            m_receivedNewSized = false;
            m_avgPtsTicks = 0; /* measured again from the new set of shared buffers */
            /* idle old-sized frames: neither in decoder queue, nor in acquired
             * safe to be destroyed
             */
            destroyEntries(DestroyMode::Idle);
            m_pending.clear();
            m_pendingDeferred.clear();
            /* non idle old-sized frames: either in decoder queue, or in acquired
             * on successible decoding, acquired, released and destroyed
             * on incompatible decuding event, released and destroyed
             */
        }

        /* Shared buffers are sized by the admitted slot count.  The local buffers that keep
         * the decoder running while all of those are held are sized by what the decoder
         * requires, which depends on the codec. */
        m_localCount = ev.requirements.numBuffers + kExtraLocalBuffers;
        fprintf(stderr, "icapturesession: %ux%u depth %u codec %u: %u shared, %u local\n",
                m_cur.width, m_cur.height, m_cur.colorDepth, 
                (unsigned)m_cur.codec, m_sharedCount, m_localCount);

        NEXUS_VideoBufferCreateSettings settings;
        NEXUS_VideoBuffer_GetDefaultCreateSettings(&settings);
        settings.width = m_cur.width;
        settings.height = m_cur.height;
        settings.colorDepth = m_cur.colorDepth;
        settings.codec = m_cur.codec;
        settings.secureVideo = NEXUS_VideoDecoderSecureType_eUnsecure;

        for (unsigned i = 0; i < m_sharedCount; ++i)
        {
            if (!addEntry(settings, true))
            {
                GST_ERROR("%s: only %u of %u shared buffers created", __FUNCTION__, i, m_sharedCount);
                break;
            }
        }
        for (unsigned i = 0; i < m_localCount; ++i)
        {
            if (!addEntry(settings, false))
            {
                GST_ERROR("%s: only %u of %u local buffers created", __FUNCTION__, i, m_localCount);
                break;
            }
        }
        queuePending();
    }

    void onDecoded(const NEXUS_VideoDecoderOutputEvent &ev)
    {
        if (!ev.buffer)
        {
            return;
        }
        const NEXUS_VideoBufferStatus &status = ev.bufferStatus;
        const uint64_t id = status.id;

        /* The STC is in host mode, nothing else seeds it: the first picture after start or flush
         * sets it to its pts, before anyone can compare a pts with it (the frame is only
         * offered to the session below). */
        if (!m_firstFrameReceived && status.ptsValid && m_stcChannel != nullptr)
        {
            const NEXUS_Error rc = NEXUS_SimpleStcChannel_SetStc(m_stcChannel, status.pts);
            if (rc != NEXUS_SUCCESS)
            {
                GST_ERROR("%s: NEXUS_SimpleStcChannel_SetStc(%u) failed: %d", __FUNCTION__, status.pts, (int)rc);
            }
            else
            {
                GST_LOG("%s: first frame, stc set to pts %u (%u ms)", __FUNCTION__, status.pts,
                          status.pts/kPtsTicksPerMs);
                m_firstFrameReceived = true;
            }
        }

        uint32_t stc = 0;
        if(m_stcChannel != nullptr) { NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc); }

        EntryMap::iterator it = m_entries.find(id);
        if (it == m_entries.end())
        {
            GST_WARNING("%s: decoded buffer %llu not in m_entries", __FUNCTION__, (unsigned long long)id);
            return;
        }

        GST_TRACE("%s: buffer %llu (%s) picture %ux%u in buffer %ux%u depth %u pts %u (%u ms) valid %d event %s", __FUNCTION__,
                  (unsigned long long)id, it->second.shared ? "shared" : "local",
                  status.streamInfo.sourceHorizontalSize, status.streamInfo.sourceVerticalSize,
                  status.width, status.height, status.colorDepth,
                  status.pts, status.pts/kPtsTicksPerMs, status.ptsValid ? 1 : 0, eventTypeName(ev.type));

        if((status.pts -stc) < kMatureTicks)
        {
            GST_LOG("%s: buffer %llu (%s) pts %7u(stc %7u) ms valid %d too shallow or too old", __FUNCTION__, (unsigned long long)id,
                      it->second.shared ? "shared" : "local", status.pts/kPtsTicksPerMs, stc/kPtsTicksPerMs, status.ptsValid ? 1 : 0);
        }
        else 
        {
            GST_TRACE("%s: buffer %llu (%s) pts %7u(stc %7u) ms valid %d", __FUNCTION__, (unsigned long long)id,
                      it->second.shared ? "shared" : "local", status.pts/kPtsTicksPerMs, stc/kPtsTicksPerMs, status.ptsValid ? 1 : 0);
        }
        it->second.flags &= ~kDecoderWrite;
        it->second.hasPts = status.ptsValid;
        it->second.pts = status.pts;
        if (status.ptsValid &&
            (!m_havePts || static_cast<int32_t>(status.pts - m_latestPts) > 0))
        {
            m_havePts = true;
            m_latestPts = status.pts;
        }
        if (ev.type == NEXUS_VideoDecoderOutputEventType_eDecodeSuccess)
        {
            /* References are maintained, so wait for eReferenceReleased before reuse. */
            it->second.flags |= kDecoderRead;
        }

        if (!m_receivedNewSized && matchesCurrent(status.width, status.height, status.colorDepth, it->second.codec))
        {
            /* The decoder has switched to the new size: retire every old-sized buffer and
             * release the new-sized ones it bounced back while it was still switching. */
            m_receivedNewSized = true;
            GST_DEBUG("%s: first new-sized picture %ux%u, retiring old-sized buffers", __FUNCTION__, status.width, status.height);
            destroyEntries(DestroyMode::IdleOldSized);
            m_pending.insert(m_pending.end(), m_pendingDeferred.begin(), m_pendingDeferred.end());
            m_pendingDeferred.clear();
        }

        offerFrame(ev.buffer, status);

        /* Recycles the buffer now if nothing holds it (local, declined, or eDecodeAndRelease). */
        releaseFlags(id, 0, false);
    }

    /* Offer a decoded picture in a shared buffer to the session.  Local buffers are never
     * offered. */
    void offerFrame(NEXUS_VideoBufferHandle buffer, const NEXUS_VideoBufferStatus &status)
    {
        if (!status.streamInfo.valid || status.streamInfo.sourceHorizontalSize == 0 ||
            status.streamInfo.sourceVerticalSize == 0)
        {
            return; /* no picture (e.g. buffer returned undecoded) */
        }

        EntryMap::iterator it = m_entries.find(status.id);
        if (it == m_entries.end() || !it->second.shared)
        {
            return;
        }
        if (!it->second.container)
        {
            NEXUS_VideoBufferContainerCreateSettings cs;
            NEXUS_VideoBufferContainer_GetDefaultCreateSettings(&cs);
            /* The container must match the buffer it holds */
            cs.bufferSettings.width = status.width;
            cs.bufferSettings.height = status.height;
            cs.bufferSettings.colorDepth = status.colorDepth;
            cs.bufferSettings.codec = m_cur.codec;
            cs.bufferSettings.secureVideo = NEXUS_VideoDecoderSecureType_eUnsecure;
            NEXUS_VideoBufferContainerHandle container = NEXUS_VideoBufferContainer_Create(&cs);
            if (!container)
            {
                GST_ERROR("%s: NEXUS_VideoBufferContainer_Create failed for buffer %llu", __FUNCTION__, (unsigned long long)status.id);
                return;
            }
            if (NEXUS_VideoBufferContainer_SetBuffer(container, buffer) != NEXUS_SUCCESS)
            {
                GST_ERROR("%s: NEXUS_VideoBufferContainer_SetBuffer failed for buffer %llu", __FUNCTION__, (unsigned long long)status.id);
                NEXUS_VideoBufferContainer_Destroy(container);
                return;
            }
            it->second.container = container;
        }

        /* Pictures are delivered in decoding order: while earlier ones wait for a slot, this
         * one waits behind them. */
        if (m_offering.empty() && deliver(status) != OfferResult::Full)
        {
            return;
        }
        it = m_entries.find(status.id);
        if (it != m_entries.end())
        {
            it->second.flags |= kOffering; /* keeps the buffer from being recycled */
            m_offering.push_back(Offering{status.id, status});
            GST_DEBUG("%s: no free slot, buffer %llu waits in m_offering (%u waiting)", __FUNCTION__,
                      (unsigned long long)status.id, (unsigned)m_offering.size());
        }
    }

    /* Hand a decoded picture to the session.  Accepted: the buffer stays held (kHeld).
     * Declined or Full: kHeld is dropped again, the caller decides what happens to the buffer. */
    OfferResult deliver(const NEXUS_VideoBufferStatus &status)
    {
        EntryMap::iterator it = m_entries.find(status.id);
        if (it == m_entries.end())
        {
            return OfferResult::Declined;
        }
        it->second.flags |= kHeld;
        const OfferResult result = m_onFrame(status.id, status);
        if (result != OfferResult::Accepted)
        {
            GST_DEBUG("%s: buffer %llu %s by session", __FUNCTION__, (unsigned long long)status.id,
                      result == OfferResult::Full ? "not accepted, session full" : "declined");
            it = m_entries.find(status.id);
            if (it != m_entries.end())
            {
                it->second.flags &= ~kHeld;
            }
        }
        return result;
    }

    /* Called on every health check: offer the waiting pictures again, oldest first, until the
     * session is full again.  Slots free up as the MW releases the frames of the old resolution
     * (their old-sized buffers are destroyed then), after which only the new set remains. */
    void drainOffering()
    {
        while (!m_offering.empty())
        {
            const Offering offering = m_offering.front();
            EntryMap::iterator it = m_entries.find(offering.id);
            if (it == m_entries.end())
            {
                GST_ERROR("%s: buffer %llu is not found in entries!", __FUNCTION__,
                          (unsigned long long)offering.id);
                m_offering.pop_front(); /* buffer is gone */
                continue;
            }
            const OfferResult result = deliver(offering.status);
            if (result == OfferResult::Full)
            {
                break;
            }
            m_offering.pop_front();
            it = m_entries.find(offering.id);
            if (it != m_entries.end())
            {
                it->second.flags &= ~kOffering;
            }
            if (result == OfferResult::Declined)
            {
                releaseFlags(offering.id, 0, false); /* recycle it now */
            }
            else
            {
                GST_DEBUG("%s: buffer %llu delivered from m_offering (%u still waiting)", __FUNCTION__,
                          (unsigned long long)offering.id, (unsigned)m_offering.size());
            }
        }
    }

    /* Give up every waiting picture and let its buffer go back to the decoder. */
    void dropOffering()
    {
        while (!m_offering.empty())
        {
            const uint64_t id = m_offering.front().id;
            m_offering.pop_front();
            EntryMap::iterator it = m_entries.find(id);
            if (it != m_entries.end())
            {
                it->second.flags &= ~kOffering;
            }
            releaseFlags(id, 0, false);
        }
    }

    /* Clear usage flags on a buffer and recycle or retire it once nothing uses it. */
    void releaseFlags(uint64_t id, unsigned clear, bool incompatible)
    {
        EntryMap::iterator it = m_entries.find(id);
        if (it == m_entries.end())
        {
            return;
        }
        Entry &e = it->second;
        e.flags &= ~clear;
        if (e.flags != 0)
        {
            return;
        }

        const bool current = matchesCurrent(e.width, e.height, e.colorDepth, e.codec);
        GST_TRACE("%s: buffer %2llu idle, current %d incompatible %d", __FUNCTION__, (unsigned long long)id, current ? 1 : 0, incompatible ? 1 : 0);
        if (!current && (m_receivedNewSized || incompatible))
        {
            destroyEntry(e);
            m_entries.erase(it);
        }
        else if (!e.shared)
        {
            /* An idle local buffer stays in m_entries, not in the lists, until
             * queueLocalThrottling() queues it. */
            return;
        }
        else if (current && incompatible && !m_receivedNewSized)
        {
            /* The decoder is not ready for the new size yet; try again once it is. */
            m_pendingDeferred.push_back(id);
        }
        else
        {
            m_pending.push_back(id);
        }
        queuePending();
    }

    bool queueEntry(Entry &e)
    {
        if (NEXUS_SimpleVideoDecoder_QueueOutputBuffer(m_decoder, e.buffer) != NEXUS_SUCCESS)
        {
            GST_WARNING("%s: queueing buffer %llu to decoder failed", __FUNCTION__, (unsigned long long)e.id);
            return false;
        }
        GST_TRACE("%s: queued %s buffer %llu(0x%llx)", __FUNCTION__, e.shared ? "shared" : "local", (unsigned long long)e.id, (unsigned long long)e.id);
        e.flags |= kDecoderWrite;
        e.hasPts = false;
        return true;
    }

    /* Buffers queued to the decoder, in total and the shared and local ones. */
    void countQueued(unsigned &queued, unsigned &sharedQueued, unsigned &localQueued) const
    {
        queued = sharedQueued = localQueued = 0;
        for (EntryMap::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
        {
            if (it->second.flags & kDecoderWrite)
            {
                ++queued;
                if (it->second.shared)
                {
                    ++sharedQueued;
                }
                else
                {
                    ++localQueued;
                }
            }
        }
    }

    /* Hand idle shared buffers (m_pending holds shared buffers only) to the decoder. */
    void queuePending()
    {
        if (!m_decoder)
        {
            return;
        }
        unsigned queued, sharedQueued, localQueued;
        countQueued(queued, sharedQueued, localQueued);

        for (std::deque<uint64_t>::iterator p = m_pending.begin();
             p != m_pending.end() && queued < kMaxQueuedBuffers;)
        {
            EntryMap::iterator it = m_entries.find(*p);
            if (it == m_entries.end() || it->second.flags != 0)
            {
                p = m_pending.erase(p);
                continue;
            }
            if (!queueEntry(it->second))
            {
                /* Decoder is stopping or restarting; keep the buffer for the next pass. */
                return;
            }
            p = m_pending.erase(p);
            ++queued;
        }
    }

    /* Average pts distance between the shared buffers (45kHz ticks), measured once per
     * configuration: when this is needed every shared buffer is held, so they all hold a
     * decoded picture.  0 if it can not be told yet. */
    int32_t sharedPtsDistance()
    {
        if (m_avgPtsTicks != 0)
        {
            return m_avgPtsTicks;
        }
        unsigned n = 0;
        uint32_t base = 0;
        int32_t lo = 0, hi = 0;
        for (EntryMap::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
        {
            const Entry &e = it->second;
            if (!e.shared || !(e.flags & (kHeld | kOffering)) || !e.hasPts)
            {
                continue;
            }
            if (n == 0)
            {
                base = e.pts;
            }
            /* relative to the first one, so that the order of decoding does not matter */
            const int32_t rel = static_cast<int32_t>(e.pts - base);
            lo = std::min(lo, rel);
            hi = std::max(hi, rel);
            ++n;
        }
        if (n >= 2 && hi > lo)
        {
            m_avgPtsTicks = (hi - lo) / static_cast<int32_t>(n - 1);
        }
        return m_avgPtsTicks;
    }

    /* Depth of the decoded pictures ahead of the STC (45kHz ticks, negative when behind).
     * False when there is nothing to measure it with. */
    bool decodedDepth(int32_t &depth) const
    {
        if (!m_havePts || !m_stcChannel)
        {
            return false;
        }
        uint32_t stc;
        NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc);
        depth = static_cast<int32_t>(m_latestPts - stc);
        return true;
    }

    /* Helper function to get current time in milliseconds */
    long long getCurrentTimeMs(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (long long)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
    }

    /* Runs on every pass of the capture thread (about every 16ms), with m_lock held.
     * Measures whether the decoder is kept running: it is when a shared buffer is queued or
     * waiting to be.  Only when every shared buffer is taken, and the decoded pictures
     * (pts) are less than DECODING_DEPTH_LOCAL_BUFFERS ahead of the STC, local buffers are
     * queued to keep up with it.  While shared buffers are released in time this does
     * nothing. */
    void healthCheck()
    {
        if (!m_decoder)
        {
            return;
        }

        /* Let the session purge frames the client is too slow to acquire, or does not acquire
         * at all (not connected yet), so their buffers go back to the decoder. */
        if (m_onHealthCheck)
        {
            m_onHealthCheck();
        }

        /* Pictures that found no free session slot, e.g. of the new resolution while the slots
         * still hold the old one. */
        drainOffering();

        long long currentTime = getCurrentTimeMs();
        if ((currentTime - m_healthCheckedMs) > (10*1000))
        {
            m_healthCheckedMs = currentTime;
            uint32_t stc = 0;
            if (m_stcChannel)
            {
                NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc);
            }
            for (EntryMap::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
            {
                const Entry &e = it->second;
                GST_TRACE("%s: %s buffer %llu flags %u (write %u read %u held %u) "
                                "%ux%u depth %u codec %u pts %7u%s(stc %7u)\n",
                        __FUNCTION__, e.shared ? "shared" : " local", (unsigned long long)e.id, e.flags, (e.flags & kDecoderWrite) ? 1 : 0,
                        (e.flags & kDecoderRead) ? 1 : 0, (e.flags & kHeld) ? 1 : 0, e.width, e.height,
                        e.colorDepth, (unsigned)e.codec, e.pts/kPtsTicksPerMs , e.hasPts ? " ms" : " (none)", stc/kPtsTicksPerMs);
            }
        }

        /* Shared buffers left waiting for a decoder queue slot (m_pending) are only handed
         * over when a buffer is released.  With every shared buffer held and nothing
         * releasing them (e.g. no client acquiring yet) nobody would drain them, and
         * waiting on m_pending here would stall the decoder for good.  Drain it now, then
         * judge by what the decoder really has queued. */
        queuePending();

        unsigned queued, sharedQueued, localQueued;
        countQueued(queued, sharedQueued, localQueued);
        if (sharedQueued != 0)
        {
            GST_TRACE("%s: shared buffers of %d in queue (pending %d)", __FUNCTION__, sharedQueued, (int)m_pending.size());
            return; /* shared buffers are available to the decoder */
        }

        int32_t depth;
        if (decodedDepth(depth))
        {
            GST_TRACE("%s: depth %7d", __FUNCTION__, depth);
            if (depth >= DECODING_DEPTH_LOCAL_BUFFERS)
            {
                return; /* far enough ahead of the STC */
            }
            if (depth < kMatureTicks)
            {
                static long long lastLoggedTime = getCurrentTimeMs();
                long long currentTime = getCurrentTimeMs();
                if( currentTime - lastLoggedTime > 1000)
                {
                    uint32_t stc = 0;
                    if (m_stcChannel)
                    {
                        NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc);
                    }
                    GST_LOG("%s: depth %7d ms too shallow or too old, last pts %7u(stc %7u) ms", __FUNCTION__, 
                            static_cast<int32_t>(depth/static_cast<int64_t>(kPtsTicksPerMs)), m_latestPts/kPtsTicksPerMs, stc/kPtsTicksPerMs);
                    lastLoggedTime = currentTime;
                }
            }
        }
#if 0
        else if (localQueued != 0)
        {
            GST_TRACE("%s: local buffers of %d in queue", __FUNCTION__, localQueued);
            return; /* nothing to pace by: one local buffer at a time */
        }
#endif

        /* local buffer should available, otherwise wait one second and report 
         * extremely exceptional case just to avoid flood logs check every 1s 
         */
#if 0
        const Clock::time_point now = Clock::now();
        if (m_nextLocalCheckValid && now < m_nextLocalCheck)
        {
            return; /* no local buffer was available; waiting before looking again */
        }
        m_nextLocalCheckValid = false;
#endif

        /* keep pacing with stc with local buffers */
        queueLocalThrottling(queued, localQueued);
    }

    /* How many more local buffers to queue to keep the latest decoded pts
     * DECODING_DEPTH_LOCAL_BUFFERS ahead of the STC. */
    unsigned localBuffersWanted(unsigned localQueued)
    {
        int32_t depth;
        if (!decodedDepth(depth))
        {
            return localQueued == 0 ? 1 : 0;
        }
        const int32_t budget = DECODING_DEPTH_LOCAL_BUFFERS - depth;
        int32_t distance = sharedPtsDistance();
        if (distance <= 0)
        {
            distance = kMatureTicks;
        }
        const int32_t frames = budget > 0 ? budget / distance : 0;
        return frames > static_cast<int32_t>(localQueued) ? frames - localQueued : 0;
    }

    /* Queue as many idle local buffers as keep the decoder DECODING_DEPTH_LOCAL_BUFFERS
     * ahead of the STC.  Local buffers are presumed idle and available, as the HW decoder is
     * fast enough to return them; if one is missing, log them all and let healthCheck()
     * wait a second before it tries again. */
    void queueLocalThrottling(unsigned queued, unsigned localQueued)
    {
        unsigned want = localBuffersWanted(localQueued);
        bool failed = false;
        for (EntryMap::iterator it = m_entries.begin();
             it != m_entries.end() && want > 0 && queued < kMaxQueuedBuffers; ++it)
        {
            Entry &e = it->second;
            if (e.shared || e.flags != 0 ||
                !(matchesCurrent(e.width, e.height, e.colorDepth, e.codec) || !m_receivedNewSized))
            {
                continue;
            }
            if (!queueEntry(e))
            {
                failed = true; /* decoder is stopping or restarting */
                break;
            }
            ++queued;
            --want;
        }
        if (want > 0 && queued < kMaxQueuedBuffers && !failed)
        {
            static long long lastLoggedTime = getCurrentTimeMs();
            long long currentTime = getCurrentTimeMs();
            /* Prevent from log flooding */
            if( currentTime - lastLoggedTime > 1000)
            {
                lastLoggedTime = currentTime;

                NEXUS_VideoDecoderStatus videoStatus;
                videoStatus.fifoDepth= 0;
                if ( NEXUS_SimpleVideoDecoder_GetStatus(m_decoder, &videoStatus) )
                {
                    GST_ERROR("Error NEXUS_SimpleVideoDecoder_GetStatus!");
                    videoStatus.fifoDepth= 0;
                }

                fprintf(stderr, "icapturesession: no idle local buffer to queue (%u more wanted) fifo level (%d)\n", want, videoStatus.fifoDepth);

                /* Only when decoder has enough data to decode, dump local buffer to determine any issue pacing with stc */
                if( videoStatus.fifoDepth >= 16 ) 
                {
                    uint32_t stc = 0;
                    if(m_stcChannel != nullptr) { NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc); }
                    for (EntryMap::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
                    {
                        const Entry &e = it->second;
                        if (!e.shared)
                        {
                            fprintf(stderr, "icapturesession:   local buffer %llu flags %u (write %u read %u held %u) "
                                            "%ux%u depth %u codec %u pts %7u%s(stc %7u)\n",
                                    (unsigned long long)e.id, e.flags, (e.flags & kDecoderWrite) ? 1 : 0,
                                    (e.flags & kDecoderRead) ? 1 : 0, (e.flags & kHeld) ? 1 : 0, e.width, e.height,
                                    e.colorDepth, (unsigned)e.codec, e.pts/kPtsTicksPerMs , e.hasPts ? " ms" : " (none)", stc/kPtsTicksPerMs);
                        }
                    }
                }
            }
        }
    }

    /* Export a shared buffer's luma/chroma memory as dma-buf fds, once, following
     * TextureData::BuildDmaBufDescriptor() in dme.  The fds stay in the entry. */
    CaptureStatus exportEntry(Entry &e)
    {
        /* The memory block (and its fd) of a buffer never changes, but the plane layout can:
         * see Layout.  The descriptor is rebuilt, keeping the fd, whenever the picture now in
         * the buffer has a different layout.  A different layout is a different dma-buf object
         * to the MW, which caches imports by object id, so it gets a new id; otherwise the MW
         * would keep drawing with the image it imported for the old layout. */
        NEXUS_VideoBufferMemoryBlocks blocks;
        NEXUS_Error err = NEXUS_VideoBufferContainer_CloneMemoryBlocks(e.container, &blocks);
        if (err != NEXUS_SUCCESS)
        {
            GST_ERROR("%s: CloneMemoryBlocks failed for buffer %llu, err %d", __FUNCTION__, (unsigned long long)e.id, (int)err);
            return err == NEXUS_NOT_AVAILABLE ? CaptureStatus::NO_FRAME : CaptureStatus::NO_RESOURCES;
        }
        const NEXUS_VideoBufferStatus &st = blocks.bufferStatus;

        Layout layout;
        layout.lumaStripeHeight = st.lumaStripeHeight;
        layout.chromaStripeHeight = st.chromaStripeHeight;
        layout.lumaStripeNPX = st.lumaStripeNPX;
        layout.chromaStripeNPX = st.chromaStripeNPX;
        layout.lumaBlockOffset = st.lumaBlockOffset;
        layout.chromaBlockOffset = st.chromaBlockOffset;
        if (e.exp.done && layout == e.exp.layout)
        {
            freeBlocks(blocks);
            return CaptureStatus::OK; /* the cached descriptor still describes this picture */
        }

        uint32_t fourcc;
        uint64_t modifier;
        if (!chromaFourcc(st.chromaPixelFormat, fourcc) || !platformSandModifier(modifier))
        {
            GST_ERROR("%s: unsupported chroma format %d or sand modifier for buffer %llu", __FUNCTION__, (int)st.chromaPixelFormat, (unsigned long long)e.id);
            freeBlocks(blocks);
            return CaptureStatus::UNSUPPORTED;
        }

        /* Luma and chroma must share one memory block (st.chromaBlockOffset locates the
         * chroma plane inside it): a buffer is one dma-buf object with one fd. */
        if (blocks.chroma != nullptr)
        {
            fprintf(stderr, "icapturesession: buffer %llu has a separate chroma memory block, unsupported\n",
                    (unsigned long long)e.id);
            freeBlocks(blocks);
            return CaptureStatus::UNSUPPORTED;
        }

        int fd = e.exp.fd;
        if (fd < 0 && NEXUS_Platform_CreateMemoryBlockDescriptor_driver(blocks.luma, 0, 0, &fd) != NEXUS_SUCCESS)
        {
            GST_ERROR("%s: CreateMemoryBlockDescriptor failed for buffer %llu", __FUNCTION__, (unsigned long long)e.id);
            freeBlocks(blocks);
            return CaptureStatus::NO_RESOURCES;
        }

        unsigned lumaPitch = st.lumaStripeHeight;
        unsigned chromaPitch = st.chromaStripeHeight;
        if (modifier == DRM_FORMAT_MOD_BROADCOM_SAND_256_MAPB1 ||
            modifier == DRM_FORMAT_MOD_BROADCOM_SAND_256_MAPB2)
        {
            lumaPitch = st.lumaStripeNPX * 256;
            chromaPitch = st.chromaStripeNPX * 256;
        }

        if (e.exp.done)
        {
            GST_DEBUG("%s: buffer %llu layout changed: pitch %u -> %u, chroma offset %llu -> %u, stripe height %u -> %u; object id %llx -> new",
                      __FUNCTION__, (unsigned long long)e.id, e.exp.lumaPitch, lumaPitch,
                      (unsigned long long)e.exp.chromaOffset, st.chromaBlockOffset,
                      e.exp.layout.lumaStripeHeight, st.lumaStripeHeight, (unsigned long long)e.exp.objectId);
            e.exp.objectId = m_nextObjectId++;
        }
        else
        {
            e.exp.objectId = e.id; /* the first layout keeps the decoder buffer id */
        }
        e.exp.layout = layout;
        e.exp.fd = fd;
        e.exp.fourcc = fourcc;
        e.exp.modifier = modifier;
        e.exp.lumaSize = blocks.lumaSize;
        e.exp.chromaSize = blocks.chromaSize;
        e.exp.lumaOffset = st.lumaBlockOffset;
        e.exp.chromaOffset = st.chromaBlockOffset;
        e.exp.lumaPitch = lumaPitch;
        e.exp.chromaPitch = chromaPitch;
        e.exp.done = true;
        GST_DEBUG("%s: buffer %llu object %llx exported fd %d fourcc 0x%x modifier 0x%llx luma %llu@%llu pitch %u chroma %llu@%llu pitch %u", __FUNCTION__,
                  (unsigned long long)e.id, (unsigned long long)e.exp.objectId, fd, fourcc, (unsigned long long)modifier,
                  (unsigned long long)e.exp.lumaSize, (unsigned long long)e.exp.lumaOffset, e.exp.lumaPitch,
                  (unsigned long long)e.exp.chromaSize, (unsigned long long)e.exp.chromaOffset, e.exp.chromaPitch);
        freeBlocks(blocks);
        return CaptureStatus::OK;
    }

    /* Describe a cached export in "frame" with a duplicated fd. */
    static CaptureStatus fillFrame(uint64_t id, const Export &x, CapturedFrame &frame)
    {
        /* A NEXUS_VideoBuffer is one dma-buf object, identified by its object id: its buffer id,
         * stable while the layout stays the same (the buffer is recycled), so the MW can
         * recognise a dma-buf it has already imported.  Both planes share it. */
        int fd = dup(x.fd);
        if (fd < 0)
        {
            GST_ERROR("%s: dup of fd %d failed for buffer %llu", __FUNCTION__, x.fd, (unsigned long long)id);
            return CaptureStatus::NO_RESOURCES;
        }

        try
        {
            ExportedDmaBuf exported;
            exported.dmaBufObjectId = id;
            exported.fd = fd;
            frame.exportedDmaBufs.push_back(exported);
            /* The object is the whole memory block: it must cover every plane's
             * offset + length (chroma lies after luma), not just the luma size. */
            const uint64_t lumaEnd = x.lumaOffset + x.lumaSize;
            const uint64_t chromaEnd = x.chromaOffset + x.chromaSize;
            frame.dmaBufObjects.push_back({id, lumaEnd > chromaEnd ? lumaEnd : chromaEnd});
            frame.planes.push_back({0, id, x.lumaOffset, x.lumaSize, x.lumaPitch});
            frame.planes.push_back({1, id, x.chromaOffset, x.chromaSize, x.chromaPitch});
        }
        catch (const std::bad_alloc &)
        {
            GST_ERROR("%s: out of memory describing buffer %llu", __FUNCTION__, (unsigned long long)id);
            close(fd);
            frame.exportedDmaBufs.clear();
            frame.dmaBufObjects.clear();
            frame.planes.clear();
            return CaptureStatus::NO_RESOURCES;
        }
        frame.capabilities.drmFormat = x.fourcc;
        frame.capabilities.drmModifier = x.modifier;
        return CaptureStatus::OK;
    }

    static void freeBlocks(NEXUS_VideoBufferMemoryBlocks &blocks)
    {
        if (blocks.chroma)
        {
            NEXUS_MemoryBlock_Free(blocks.chroma);
        }
        NEXUS_MemoryBlock_Free(blocks.luma);
    }

    FrameFn m_onFrame;
    HealthFn m_onHealthCheck;
    NEXUS_EventHandle m_event{nullptr};
    NEXUS_SimpleVideoDecoderHandle m_prepDecoder{nullptr};
    /* Object ids of re-laid-out buffers: far above any decoder buffer id. */
    static constexpr uint64_t kObjectIdBase = 1ULL << 48;
    uint64_t m_nextObjectId{kObjectIdBase};

    std::thread m_thread;
    volatile bool m_quit{false};
    bool m_running{false};

    /* Guards everything below.  Recursive because offering a frame can make the session
     * release another buffer, and that comes straight back into releaseBuffer().  The
     * session never calls into this class while holding its own mutex. */
    std::recursive_mutex m_lock;
    NEXUS_SimpleVideoDecoderHandle m_decoder{nullptr};
    NEXUS_VideoDecoderSettings m_videoDecoderSettings;
    Requirements m_cur;
    unsigned m_sharedCount{0};
    unsigned m_localCount{0};
    NEXUS_SimpleStcChannelHandle m_stcChannel{nullptr};
    bool m_havePts{false};
    bool m_firstFrameReceived{false}; /* the STC was set from the first picture since start/flush */
    uint32_t m_latestPts{0};   /* latest pts decoded, shared or local buffer */
    int32_t m_avgPtsTicks{0};  /* see sharedPtsDistance() */
    typedef std::chrono::steady_clock Clock;
    bool m_nextLocalCheckValid{false}; /* set after finding no idle local buffer, extremely exceptional */
    Clock::time_point m_nextLocalCheck;
    bool m_haveRequirements{false};
    bool m_receivedNewSized{true};
    EntryMap m_entries;
    std::deque<uint64_t> m_pending;  /* idle buffers waiting for a free decoder queue slot */
    std::deque<uint64_t> m_pendingDeferred; /* new-sized buffers the decoder rejected during a switch */
    struct Offering
    {
        uint64_t id;
        NEXUS_VideoBufferStatus status;
    };
    /* Decoded pictures (kOffering) the session had no free slot for, in decoding order. */
    std::deque<Offering> m_offering;
    long long m_healthCheckedMs{0};
};

/* Effective colour interpretation (spec 6.2).  Unspecified matrix/range fall back to the
 * conventional defaults for the picture size (BT.601 below 720 lines, BT.709 above) and
 * narrow range.  The decoder does not report chroma siting: 4:2:0 streams are assumed
 * horizontally co-sited, vertically midpoint (MPEG/H.26x default). */
CaptureColorDescription colorDescription(const NEXUS_VideoDecoderStreamInformation &info)
{
    CaptureColorDescription c;
    switch (info.matrixCoefficients)
    {
    case NEXUS_MatrixCoefficients_eItu_R_BT_709:
    case NEXUS_MatrixCoefficients_eXvYCC_709:
        c.matrix = CaptureYcbcrMatrix::BT709;
        break;
    case NEXUS_MatrixCoefficients_eFCC:
    case NEXUS_MatrixCoefficients_eItu_R_BT_470_2_BG:
    case NEXUS_MatrixCoefficients_eSmpte_170M:
    case NEXUS_MatrixCoefficients_eXvYCC_601:
        c.matrix = CaptureYcbcrMatrix::BT601;
        break;
    case NEXUS_MatrixCoefficients_eItu_R_BT_2020_NCL:
    case NEXUS_MatrixCoefficients_eItu_R_BT_2020_CL:
        c.matrix = CaptureYcbcrMatrix::BT2020;
        break;
    default:
        c.matrix = (info.sourceVerticalSize >= 720) ? CaptureYcbcrMatrix::BT709 : CaptureYcbcrMatrix::BT601;
        break;
    }
    c.range = (info.colorRange == NEXUS_ColorRange_eFull) ? CaptureSampleRange::FULL : CaptureSampleRange::NARROW;
    c.horizontalSiting = CaptureChromaSiting::COSITED;
    c.verticalSiting = CaptureChromaSiting::MIDPOINT;
    return c;
}

/* Live sessions of this library.  The sink receives an arbitrary pointer through the
 * rdk-capture-session property; it is only dereferenced after being found here. */
std::mutex g_liveSessionsMutex;
std::vector<const void *> g_liveSessions;

class BrcmCaptureSession final : public ICaptureSession
{
public:
    BrcmCaptureSession()
        : m_capture([this](uint64_t bufferId, const NEXUS_VideoBufferStatus &status) {
              return offerFrame(bufferId, status);
          },
          [this]() { onHealthCheck(); })
    {
        std::lock_guard<std::mutex> lock(g_liveSessionsMutex);
        g_liveSessions.push_back(static_cast<const ICaptureSession *>(this));
    }

    CaptureStatus getCapabilities(CaptureCapabilities &out) const override
    {
        {
            /* Valid from connect() (configured READY, spec 4.4) */
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_connected)
            {
                GST_WARNING("%s: not connected", __FUNCTION__);
                return CaptureStatus::INVALID_ARGUMENT;
            }
        }
        uint64_t modifier;
        if (!platformSandModifier(modifier))
        {
            GST_ERROR("%s: no sand modifier for this platform", __FUNCTION__);
            return CaptureStatus::UNSUPPORTED;
        }
        CaptureCapabilities caps;
        caps.drmFormat = DRM_FORMAT_NV21; /* this decoder emits Cb8_Cr8; 10-bit streams are reported per frame as P030 */
        caps.drmModifier = modifier;
        caps.maximumContentSize = {kMaxWidth, kMaxHeight};
        caps.minimumSlots = kMinSlots;
        caps.maximumSlots = kMaxSlots;
        out = caps;
        return CaptureStatus::OK;
    }

    CaptureStatus createPool(uint32_t slotCount, CapturePool &outPool) override
    {
        uint64_t dropped[kMaxSlots];
        unsigned numDropped = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            GST_DEBUG("%s: slotCount %u", __FUNCTION__, slotCount);
            if (m_fatal)
            {
                GST_ERROR("%s: session is fatal", __FUNCTION__);
                return CaptureStatus::FATAL_ERROR;
            }
            if (!m_connected)
            {
                GST_WARNING("%s: not connected", __FUNCTION__);
                return CaptureStatus::INVALID_ARGUMENT;
            }
            /* Repeating the admitted configuration is rejected; a different count is a
             * readmission and leaves frames the MW still holds valid. */
            if (slotCount < kMinSlots || slotCount > kMaxSlots ||
                (m_poolCreated && slotCount == m_admittedSlots))
            {
                GST_WARNING("%s: invalid slot count %u (min %u max %u admitted %u)", __FUNCTION__, slotCount,
                          kMinSlots, kMaxSlots, (unsigned)m_admittedSlots);
                return CaptureStatus::INVALID_ARGUMENT;
            }

            /* Slots hold decoder pictures, which have no fixed backing: there are no pool
             * dma-bufs and the per-slot layout is delivered with each acquired frame. */
            CapturePool pool;
            const size_t oldSize = m_slots.size();
            try
            {
                pool.backingSize = {kMaxWidth, kMaxHeight};
                for (uint32_t i = 0; i < slotCount; ++i)
                {
                    CaptureSlotLayout layout;
                    layout.slotIndex = i;
                    /* Zero-ID templates: NV12/P030 luma and chroma planes.  The real object
                     * ids, offsets and strides are delivered with each acquired frame. */
                    layout.planes.push_back({0, 0, 0, 0, 0});
                    layout.planes.push_back({1, 0, 0, 0, 0});
                    pool.slotLayouts.push_back(std::move(layout));
                }
                if (m_slots.size() < slotCount)
                {
                    m_slots.resize(slotCount);
                }
            }
            catch (const std::bad_alloc &)
            {
                GST_ERROR("%s: out of memory creating %u slots", __FUNCTION__, slotCount);
                m_slots.resize(oldSize);
                return CaptureStatus::NO_RESOURCES;
            }

            /* Slots outside the new admission stop being selectable; frames the MW holds
             * there stay valid until released. */
            for (uint32_t i = slotCount; i < m_slots.size(); ++i)
            {
                if (!m_slots[i].locked && m_slots[i].occupied)
                {
                    dropped[numDropped++] = m_slots[i].bufferId;
                    m_slots[i].occupied = false;
                }
            }
            m_admittedSlots = slotCount;
            m_poolCreated = true;
            outPool = std::move(pool);
        }
        for (unsigned i = 0; i < numDropped; ++i)
        {
            m_capture.releaseBuffer(dropped[i]);
        }
        /* One shared decoder buffer per slot; applies from the next requirements event. */
        m_capture.setSharedCount(slotCount);
        return CaptureStatus::OK;
    }

    CaptureStatus acquireCurrentFrame(CapturedFrame &outFrame) override
    {
        if (outFrame.slotIndex != kInvalidCaptureSlotIndex ||
            outFrame.acquisitionToken != kInvalidAcquisitionToken)
        {
            GST_WARNING("%s: outFrame already holds slot %u", __FUNCTION__, (unsigned)outFrame.slotIndex);
            return CaptureStatus::INVALID_ARGUMENT;
        }
        for (const ExportedDmaBuf &e : outFrame.exportedDmaBufs)
        {
            if (e.fd != -1)
            {
                GST_WARNING("%s: outFrame already holds an fd", __FUNCTION__);
                return CaptureStatus::INVALID_ARGUMENT;
            }
        }
        if (!outFrame.planes.empty() || !outFrame.dmaBufObjects.empty())
        {
            GST_WARNING("%s: outFrame is not empty", __FUNCTION__);
            return CaptureStatus::INVALID_ARGUMENT;
        }

        uint32_t index = kInvalidCaptureSlotIndex;
        uint64_t bufferId = 0;
        CaptureStatus early = CaptureStatus::OK;
        ReleaseList purged;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_poolCreated)
            {
                /* createPool() must precede acquisition (spec 7.3) */
                GST_WARNING("%s: no pool created yet", __FUNCTION__);
                early = CaptureStatus::INVALID_ARGUMENT;
            }
            else if (m_fatal)
            {
                early = CaptureStatus::FATAL_ERROR;
            }
            else if (m_unsupported)
            {
                early = CaptureStatus::UNSUPPORTED;
            }
            else
            {
                /* Frames the STC has overtaken are dropped before one is selected. */
                purgeOldFramesLocked(purged);

                /* A frame with a new backing is about to be read: frames of the old backing
                 * are all supposed to be read out,  no longer wait to release those. */
                const uint32_t changed = findBackingChangeLocked();
                if (changed != kInvalidCaptureSlotIndex)
                {
                    purgeObsoleteFramesLocked(changed, purged);
                    m_slots[changed].backingChanged = false;
                }

                /* The selection is the unacquired frame with the smallest pts. */
                index = findSelectionLocked();

                /* Platform output shows a frame only when the STC reaches its pts.  A frame
                 * still ahead of the STC is not the current selection: the one acquired last
                 * remains it (NO_NEW_FRAME), or there is none yet (NO_FRAME).  Without a usable
                 * STC the due time can not be judged and the oldest frame is taken. */
                uint32_t stc = 0;
                if (index != kInvalidCaptureSlotIndex && currentStcLocked(stc) && !isDue(m_slots[index], stc))
                {
                    GST_TRACE("%s: slot %2u buffer %llu pts %7u ms not due yet (stc %7u ms)", __FUNCTION__, index,
                              (unsigned long long)m_slots[index].bufferId, m_slots[index].pts/kPtsTicksPerMs,
                              stc/kPtsTicksPerMs);
                    index = kInvalidCaptureSlotIndex;
                }
                if (index == kInvalidCaptureSlotIndex)
                {
                    bool allLocked = !m_slots.empty();
                    for (const Slot &sl : m_slots)
                    {
                        allLocked = allLocked && sl.locked;
                    }
                    /* Every slot held by the MW: decode is stalled for lack of a free slot */
                    early = allLocked ? CaptureStatus::NO_RESOURCES
                                      : (m_everAcquired ? CaptureStatus::NO_NEW_FRAME : CaptureStatus::NO_FRAME);
                }
                else
                {
                    /* Temporary lock: keeps the slot from being replaced while it is exported. */
                    m_slots[index].locked = true;
                    bufferId = m_slots[index].bufferId;
                }
            }
        }
        purged.releaseAll(m_capture);
        if (early != CaptureStatus::OK)
        {
            GST_TRACE("%s: no frame acquired, status %d", __FUNCTION__, (int)early);
            return early;
        }

        /* The session mutex is not held while calling into BufferCapture (lock order). */
        CapturedFrame frame;
        const CaptureStatus status = m_capture.exportFrame(bufferId, frame);

        uint64_t releaseId = 0;
        bool release = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            Slot &slot = m_slots[index];
            if (status != CaptureStatus::OK)
            {
                /* Roll back: nothing is committed.  Only a descriptor shortage is retriable;
                 * otherwise this picture can never be exported, so drop it and let the next
                 * one be selected. */
                GST_ERROR("%s: exporting buffer %llu failed, status %d", __FUNCTION__, (unsigned long long)bufferId, (int)status);
                slot.locked = false;
                if (status != CaptureStatus::NO_RESOURCES && slot.occupied)
                {
                    releaseId = slot.bufferId;
                    slot.occupied = false;
                    release = true;
                }
            }
            else
            {
                uint32_t stc = 0;
                if(m_stcChannel != nullptr) { NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc); }
                slot.token = m_nextToken++;
                slot.objectId = frame.planes.empty() ? bufferId : frame.planes[0].dmaBufObjectId;
                m_everAcquired = true;
                if(slot.token % 100 == 0) {
                    GST_DEBUG("%s: acquired slot %2u buffer %llu token %llu pts %7lld(stc %7llu) ms", __FUNCTION__, index,
                              (unsigned long long)bufferId, (unsigned long long)slot.token,
                              (long long)slot.ptsNs/1000000ULL, (unsigned long long)stc/kPtsTicksPerMs);
                }
                else 
                {
                    GST_TRACE("%s: acquired slot %2u buffer %llu token %llu pts %7lld(stc %7llu) ms", __FUNCTION__, index,
                              (unsigned long long)bufferId, (unsigned long long)slot.token,
                              (long long)slot.ptsNs/1000000ULL, (unsigned long long)stc/kPtsTicksPerMs);
                }

                outFrame.slotIndex = index;
                outFrame.acquisitionToken = slot.token;
                outFrame.presentationTimeNs = slot.ptsNs;
                outFrame.visibleRegion = slot.region;
                outFrame.dmaBufObjectsFlushed = m_objectsFlushed;
                m_objectsFlushed = false; /* cleared only by a successful acquisition */
                outFrame.capabilities = frame.capabilities;
                outFrame.capabilities.minimumSlots = kMinSlots;
                outFrame.capabilities.maximumSlots = kMaxSlots;
                outFrame.capabilities.maximumContentSize = {kMaxWidth, kMaxHeight};
                outFrame.colorDescription = slot.color;
                outFrame.backingSize = slot.backing;
                outFrame.dmaBufObjects = std::move(frame.dmaBufObjects);
                outFrame.planes = std::move(frame.planes);
                outFrame.exportedDmaBufs = std::move(frame.exportedDmaBufs);
            }
        }
        if (release)
        {
            m_capture.releaseBuffer(releaseId);
        }
        return status;
    }

    CaptureStatus releaseFrame(CapturedFrame &frame) override
    {
        uint64_t releaseId = 0;
        bool release = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (frame.acquisitionToken == kInvalidAcquisitionToken)
            {
                GST_WARNING("%s: frame has no acquisition token", __FUNCTION__);
                return CaptureStatus::INVALID_ARGUMENT;
            }
            const uint32_t index = findHeldSlotLocked(frame);
            if (index == kInvalidCaptureSlotIndex)
            {
                /* Not held in a slot: a frame purged because its backing changed has nothing
                 * left to release, so the late release succeeds. */
                if (!forgetReleasedLocked(frame))
                {
                    fprintf(stderr, "icapturesession: releaseFrame: frame not found with bufferId %llu "
                                    "(token %llu, slot %2u): not held in any slot nor purged earlier\n",
                            (unsigned long long)(frame.planes.empty() ? 0 : frame.planes[0].dmaBufObjectId),
                            (unsigned long long)frame.acquisitionToken, (unsigned)frame.slotIndex);
                    return CaptureStatus::INVALID_ARGUMENT;
                }
                GST_DEBUG("%s: late release of purged frame, token %llu", __FUNCTION__, (unsigned long long)frame.acquisitionToken);
                frame.slotIndex = kInvalidCaptureSlotIndex;
                frame.acquisitionToken = kInvalidAcquisitionToken;
                frame.dmaBufObjectsFlushed = false;
                return CaptureStatus::OK;
            }
            Slot &slot = m_slots[index];
            if(slot.token % 100 == 0) {
                GST_DEBUG("%s: slot %2u buffer %llu token %llu", __FUNCTION__, index,
                          (unsigned long long)slot.bufferId, (unsigned long long)slot.token);
            }
            else
            {
                GST_TRACE("%s: slot %2u buffer %llu token %llu", __FUNCTION__, index,
                          (unsigned long long)slot.bufferId, (unsigned long long)slot.token);
            }
            slot.locked = false;
            slot.token = kInvalidAcquisitionToken;
            if (slot.occupied)
            {
                /* The MW is done with it: the buffer goes back to the decoder. */
                releaseId = slot.bufferId;
                slot.occupied = false;
                release = true;
            }
            frame.slotIndex = kInvalidCaptureSlotIndex;
            frame.acquisitionToken = kInvalidAcquisitionToken;
            frame.dmaBufObjectsFlushed = false;
        }
        if (release)
        {
            m_capture.releaseBuffer(releaseId);
        }
        return CaptureStatus::OK;
    }

    ~BrcmCaptureSession() override
    {
        {
            std::lock_guard<std::mutex> lock(g_liveSessionsMutex);
            g_liveSessions.erase(std::remove(g_liveSessions.begin(), g_liveSessions.end(),
                                             static_cast<const void *>(static_cast<const ICaptureSession *>(this))),
                                 g_liveSessions.end());
        }
        GST_DEBUG("%s: destroying session", __FUNCTION__);
        m_capture.stop(); /* destroys every decoder buffer, container and cached fd */
    }

    /* ---- sink side ---- */

    bool startBufferCapture(void *decoder)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_connected || m_fatal)
            {
                GST_ERROR("%s: not connected (%d) or fatal (%d)", __FUNCTION__, m_connected ? 0 : 1, m_fatal ? 1 : 0);
                return false;
            }
        }

        return m_capture.start(static_cast<NEXUS_SimpleVideoDecoderHandle>(decoder));
    }

    bool prepareBufferCapture(void *decoder)
    {
        /* The STC channel is reconfigured here, before NEXUS_SimpleVideoDecoder_Start(): changing
         * its mode or sync under a running decoder invalidates the snapshot reference the decoder
         * holds, and the decoder's Stop then asserts in nexus_stc_channel.c. Needs setStcChannel()
         * to have been called first. */
        if (m_stcChannel) // && !m_stcSettingsSaved)
        {
            NEXUS_SimpleStcChannelSettings Settings;
            NEXUS_SimpleStcChannel_GetSettings(m_stcChannel, &Settings);
            m_stcSettings = Settings; /* keep a previous setting to revert at stop */
            m_stcSettingsChannel = m_stcChannel;
            m_stcSettingsSaved = true;
            Settings.mode = NEXUS_StcChannelMode_eHost;
            Settings.sync = NEXUS_SimpleStcChannelSyncMode_eOff;
            Settings.modeSettings.Auto.behavior = NEXUS_StcChannelAutoModeBehavior_eFirstAvailable;
            NEXUS_SimpleStcChannel_SetSettings(m_stcChannel, &Settings);
        }

        return m_capture.prepare(static_cast<NEXUS_SimpleVideoDecoderHandle>(decoder));
    }

    void stopBufferCapture()
    {
        GST_DEBUG("%s", __FUNCTION__);
        m_capture.stop();

        /* Restore only what startBufferCapture() changed, on the channel it changed */
        //if (m_stcSettingsSaved)
        //{
        //    NEXUS_SimpleStcChannel_SetSettings(m_stcSettingsChannel, &m_stcSettings);
        //    m_stcSettingsSaved = false;
        //    m_stcSettingsChannel = nullptr;
        //}

        /* The decoder buffers are gone: nothing can be acquired any more.  Frames the MW
         * holds stay valid until released (their fds are the MW's own duplicates). */
        std::lock_guard<std::mutex> lock(m_mutex);
        for (Slot &slot : m_slots)
        {
            slot.occupied = false;
        }
        /* m_everAcquired is kept: the final selection stays NO_NEW_FRAME after teardown */
    }

    /* Frames dropped because acquireCurrentFrame() was not called fast enough. */
    uint64_t droppedFrames() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_droppedFrames;
    }

    /* The STC the frame pts are compared against; NULL (the default) disables purging
     * by pts. */
    void setStcChannel(void *stcChannel)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stcChannel = static_cast<NEXUS_SimpleStcChannelHandle>(stcChannel);
        }
        /* also paces the local buffers; never call into it holding m_mutex */
        m_capture.setStcChannel(static_cast<NEXUS_SimpleStcChannelHandle>(stcChannel));
    }

    bool connect()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        /* Connecting (NULL->READY) does not need a pool: the pool is created afterwards,
         * before the first acquisition; frame offers stay rejected until then. */
        if (m_connected || m_everConnected || m_fatal)
        {
            GST_WARNING("%s: refused (connected %d ever %d fatal %d)", __FUNCTION__,
                      m_connected ? 1 : 0, m_everConnected ? 1 : 0, m_fatal ? 1 : 0);
            return false;
        }
        m_connected = m_everConnected = true;
        GST_DEBUG("%s: connected", __FUNCTION__);
        
        return true;
    }

    void disconnect()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected = false;
        GST_DEBUG("%s: disconnected", __FUNCTION__);
    }

    /* Drop every frame not yet acquired (decoder flush).  A frame the MW holds is not
     * affected.  The STC is set again from the next decoded picture. */
    void flush()
    {
        ReleaseList dropped;
        GST_DEBUG("%s", __FUNCTION__);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            dropUnacquiredFramesLocked(dropped);
            m_everAcquired = false;
        }
        GST_DEBUG("%s: dropped %d", __FUNCTION__, dropped.count);
        dropped.releaseAll(m_capture);
        m_capture.flush(); /* the next decoded picture re-seeds the STC; never call in holding m_mutex */
    }

    /* Capture thread, from BufferCapture::healthCheck(): drop the frames the STC has overtaken
     * so slots do not stay filled when the client acquires too slowly or not at all. */
    void onHealthCheck()
    {
        ReleaseList purged;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_poolCreated || m_fatal)
            {
                return;
            }
            purgeOldFramesLocked(purged);
        }
        purged.releaseAll(m_capture);
    }

    /* A decoded picture arrives on the capture thread, or is offered again from
     * BufferCapture::m_offering.  Accepted if it was stored in a slot, Full if there is no free
     * slot yet; never blocks on the MW. */
    OfferResult offerFrame(uint64_t bufferId, const NEXUS_VideoBufferStatus &status)
    {
        ReleaseList purged;
        OfferResult result;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            result = offerFrameLocked(bufferId, status, purged);
        }
        purged.releaseAll(m_capture);
        return result;
    }

private:
    static constexpr uint32_t kPtsTicksPerSecond = 45000;

    struct Slot
    {
        bool occupied{false};  /* holds decoder buffer bufferId */
        uint64_t bufferId{0};
        uint64_t objectId{0};  /* dmaBufObjectId of the frame acquired from it (see BufferCapture::Export) */
        bool locked{false};
        uint64_t token{kInvalidAcquisitionToken};
        uint64_t sequence{0};  /* arrival order */
        uint32_t pts{0};       /* 45kHz */
        bool ptsValid{false};
        int64_t ptsNs{0};
        Rectangle region{};
        Size backing{};
        unsigned colorDepth{0};
        CaptureColorDescription color{}; /* from the decoder stream information */
        bool backingChanged{false}; /* first frame after a backing change */
    };

    /* A frame delivered to the MW whose buffer was released early (backing change). */
    struct ReleasedFrame
    {
        uint64_t token;
        uint64_t objectId; /* dmaBufObjectId of the delivered frame */
    };

    /* Buffers to give back to the decoder once the session mutex is dropped. */
    struct ReleaseList
    {
        uint64_t ids[kMaxSlots];
        unsigned count{0};

        void add(uint64_t id) { ids[count++] = id; }
        void releaseAll(BufferCapture &capture)
        {
            for (unsigned i = 0; i < count; ++i)
            {
                capture.releaseBuffer(ids[i]);
            }
            count = 0;
        }
    };

    /* Presentation order: by pts, or by arrival when either has no valid pts. */
    static bool earlier(const Slot &a, const Slot &b)
    {
        if (a.ptsValid && b.ptsValid && a.pts != b.pts)
        {
            return static_cast<int32_t>(a.pts - b.pts) < 0;
        }
        return a.sequence < b.sequence;
    }

    static bool isDue(const Slot &s, uint32_t stc)
    {
        if ( (stc & 0x80000000) != 0)
        {
            /* invalid stc */
            return false;
        }
        return !s.ptsValid || static_cast<int32_t>(s.pts - stc) <= kMatureTicks;
    }

    bool currentStcLocked(uint32_t &stc) const
    {
        if (!m_stcChannel)
        {
            return false;
        }
        NEXUS_SimpleStcChannel_GetStc(m_stcChannel, &stc);
        if ( (stc & 0x80000000) != 0)
        {
            /* invalid stc */
            return false;
        }
        return true;
    }

    /* An unacquired frame that is available to be selected. */
    bool selectable(const Slot &s) const { return s.occupied && !s.locked; }

    uint32_t findSelectionLocked() const
    {
        uint32_t best = kInvalidCaptureSlotIndex;
        for (uint32_t i = 0; i < m_admittedSlots && i < m_slots.size(); ++i)
        {
            if (selectable(m_slots[i]) && (best == kInvalidCaptureSlotIndex || earlier(m_slots[i], m_slots[best])))
            {
                best = i;
            }
        }
        return best;
    }

    /* The oldest unacquired frame that starts a new backing, if any. */
    uint32_t findBackingChangeLocked() const
    {
        uint32_t best = kInvalidCaptureSlotIndex;
        for (uint32_t i = 0; i < m_admittedSlots && i < m_slots.size(); ++i)
        {
            if (selectable(m_slots[i]) && m_slots[i].backingChanged &&
                (best == kInvalidCaptureSlotIndex || earlier(m_slots[i], m_slots[best])))
            {
                best = i;
            }
        }
        return best;
    }

    void dropSlotLocked(uint32_t i, ReleaseList &out)
    {
        GST_TRACE("%s: slot %2u buffer %llu", __FUNCTION__, i, (unsigned long long)m_slots[i].bufferId);
        out.add(m_slots[i].bufferId);
        m_slots[i].occupied = false;
    }

    /* Release unacquired frames that have been overtaken by the STC, so that the slots hold
     * only frames still to be shown and the one with the smallest pts is the selection.
     * The frame that is due now is the one to show; older due frames are dropped.  Frames
     * ahead of the STC stay.  Frames the MW holds are never touched. */
    void purgeOldFramesLocked(ReleaseList &out)
    {
        uint32_t stc;
        if (!currentStcLocked(stc))
        {
            return;
        }
        uint32_t newestDue = kInvalidCaptureSlotIndex;
        for (uint32_t i = 0; i < m_slots.size(); ++i)
        {
            if (selectable(m_slots[i]) && isDue(m_slots[i], stc) &&
                (newestDue == kInvalidCaptureSlotIndex || earlier(m_slots[newestDue], m_slots[i])))
            {
                newestDue = i;
            }
        }
        if (newestDue == kInvalidCaptureSlotIndex)
        {
            return;
        }
        for (uint32_t i = 0; i < m_slots.size(); ++i)
        {
            if (i != newestDue && selectable(m_slots[i]) && isDue(m_slots[i], stc) &&
                earlier(m_slots[i], m_slots[newestDue]))
            {
                /* The STC overtook it before the MW asked for it: the MW is not keeping up. */
                const Slot &s = m_slots[i];
                ++m_droppedFrames;
                GST_TRACE("%s: frame dropped (total %llu): buffer %llu %ux%u depth %u "
                                "pts %7u ms (valid %d) stc %7u ms\n", __FUNCTION__,
                        (unsigned long long)m_droppedFrames, (unsigned long long)s.bufferId,
                        s.backing.width, s.backing.height, s.colorDepth, s.pts/kPtsTicksPerMs, s.ptsValid ? 1 : 0, stc/kPtsTicksPerMs);
                dropSlotLocked(i, out);
            }
        }
    }

    /* Release every frame whose size or color depth differs from frame "given", the first
     * of a new backing is about to be acquired.
     * All of them are expected to be locked, i.e. already delivered by acquireCurrentFrame.
     * Such buffers are retired by the decoder side already, never required queued back, so frames 
     * the MW still holds could be released too instead of waiting for releaseFrame(). 
     */
    void purgeObsoleteFramesLocked(uint32_t given, ReleaseList &out)
    {
        /* Frames purged by the previous call are expected to have been released by the MW. */
        if (!m_released.empty())
        {
            fprintf(stderr, "icapturesession: %u frame(s) purged earlier were never released by the MW\n",
                    (unsigned)m_released.size());
            m_released.clear();
        }

        const Slot &ref = m_slots[given];
        for (uint32_t i = 0; i < m_slots.size(); ++i)
        {
            const Slot &s = m_slots[i];
            if (i != given && s.occupied &&
                (s.backing.width != ref.backing.width || s.backing.height != ref.backing.height ||
                 s.colorDepth != ref.colorDepth))
            {
                if (!s.locked)
                {
                    /* Old-backing frames are expected to have been delivered already. This is unexpected! */
                    ++m_droppedFrames;
                    fprintf(stderr, "icapturesession: purging unacquired frame of the old backing: buffer %llu "
                                    "%ux%u depth %u (new %ux%u depth %u); it should have been acquired\n",
                            (unsigned long long)s.bufferId, s.backing.width, s.backing.height, s.colorDepth,
                            ref.backing.width, ref.backing.height, ref.colorDepth);
                }
                if (s.locked && s.token != kInvalidAcquisitionToken)
                {
                    /* Delivered to the MW: remember it so that its release, which has nothing
                     * left to do, still succeeds, and free the slot. */
                    try
                    {
                        m_released.push_back({s.token, s.objectId});
                    }
                    catch (const std::bad_alloc &)
                    {
                        fprintf(stderr, "icapturesession: out of memory recording purged frame token %llu\n",
                                (unsigned long long)s.token);
                    }
                    m_slots[i].locked = false;
                    m_slots[i].token = kInvalidAcquisitionToken;
                }
                /* A slot locked without a token is being exported by acquireCurrentFrame(),
                 * which commits or rolls it back itself, so it stays locked. */
                dropSlotLocked(i, out);
            }
        }
    }

    /* The slot holding "frame": found by the dmaBufObjectId of its first plane (Slot::objectId), falling back to the slot index for a frame without
     * planes.  The acquisition token must match. */
    uint32_t findHeldSlotLocked(const CapturedFrame &frame) const
    {
        if (frame.planes.empty())
        {
            return kInvalidCaptureSlotIndex;
        }
        const uint64_t objectId = frame.planes[0].dmaBufObjectId;
        for (uint32_t i = 0; i < m_slots.size(); ++i)
        {
            if (m_slots[i].locked && m_slots[i].objectId == objectId &&
                m_slots[i].token == frame.acquisitionToken)
            {
                return i;
            }
        }
        return kInvalidCaptureSlotIndex;
    }

    /* Remove the record of a frame purged by purgeObsoleteFramesLocked(); false if "frame"
     * is not one. */
    bool forgetReleasedLocked(const CapturedFrame &frame)
    {
        for (std::vector<ReleasedFrame>::iterator it = m_released.begin(); it != m_released.end(); ++it)
        {
            if (it->token == frame.acquisitionToken &&
                (frame.planes.empty() || frame.planes[0].dmaBufObjectId == it->objectId))
            {
                m_released.erase(it);
                return true;
            }
        }
        return false;
    }

    /* Release every unacquired frame (flush, unsupported content).  Frames the MW holds
     * are not affected. */
    void dropUnacquiredFramesLocked(ReleaseList &out)
    {
        for (uint32_t i = 0; i < m_slots.size(); ++i)
        {
            if (selectable(m_slots[i]))
            {
                dropSlotLocked(i, out);
            }
        }
    }

    uint32_t findEmptySlotLocked() const
    {
        for (uint32_t i = 0; i < m_admittedSlots && i < m_slots.size(); ++i)
        {
            if (!m_slots[i].occupied && !m_slots[i].locked)
            {
                return i;
            }
        }
        return kInvalidCaptureSlotIndex;
    }

    OfferResult offerFrameLocked(uint64_t bufferId, const NEXUS_VideoBufferStatus &status, ReleaseList &purged)
    {
        const uint32_t width = status.streamInfo.sourceHorizontalSize;
        const uint32_t height = status.streamInfo.sourceVerticalSize;

        /* No capture publication before createPool() succeeds. */
        if (!m_connected || !m_poolCreated || m_fatal || m_unsupported || width == 0 || height == 0)
        {
            GST_DEBUG("%s: buffer %llu rejected (connected %d pool %d fatal %d unsupported %d size %ux%u)", __FUNCTION__,
                      (unsigned long long)bufferId, m_connected ? 1 : 0, m_poolCreated ? 1 : 0, m_fatal ? 1 : 0, m_unsupported ? 1 : 0, width, height);
            return OfferResult::Declined;
        }
        if (width > kMaxWidth || height > kMaxHeight)
        {
            /* Selection stops updating; acquisition reports UNSUPPORTED. */
            GST_ERROR("%s: content %ux%u exceeds maximum %ux%u, unsupported", __FUNCTION__, width, height, kMaxWidth, kMaxHeight);
            m_unsupported = true;
            dropUnacquiredFramesLocked(purged);
            return OfferResult::Declined;
        }

        /* With one resolution there is one shared decoder buffer per slot, so a free slot always
         * exists.  During a resolution change the slots still hold frames of the old resolution
         * while the decoder already decodes into the new set: the picture waits in
         * BufferCapture::m_offering until the MW releases an old frame.  Nothing was changed
         * yet, so it can be offered again as it is. */
        uint32_t target = findEmptySlotLocked();
        if (target == kInvalidCaptureSlotIndex)
        {
            GST_DEBUG("%s: no free slot for decoder buffer %llu (admitted slots %u)", __FUNCTION__,
                      (unsigned long long)bufferId, (unsigned)m_admittedSlots);
            return OfferResult::Full;
        }

        Slot &slot = m_slots[target];
        slot.occupied = true;
        slot.bufferId = bufferId;
        slot.sequence = m_nextSequence++;
        slot.ptsValid = status.ptsValid;
        slot.pts = status.ptsValid ? status.pts : 0;
        slot.ptsNs = status.ptsValid
            ? static_cast<int64_t>(static_cast<uint64_t>(status.pts) * 1000000000ULL / kPtsTicksPerSecond)
            : 0;
        GST_TRACE("%s: buffer %llu into slot %2u seq %2llu pts %7u ms valid %d", __FUNCTION__, (unsigned long long)bufferId,
                  target, (unsigned long long)slot.sequence, slot.pts/kPtsTicksPerMs, slot.ptsValid ? 1 : 0);
        slot.region = {0, 0, {width, height}};
        slot.backing = {status.width, status.height};
        slot.colorDepth = status.colorDepth;
        slot.color = colorDescription(status.streamInfo);

        /* A backing change invalidates previously exported object identities. */
        const bool backingChanged = m_haveBacking && (status.width != m_lastBacking.width ||
                                                      status.height != m_lastBacking.height ||
                                                      status.colorDepth != m_lastColorDepth);
        const Size oldBacking = m_lastBacking;
        const unsigned oldColorDepth = m_lastColorDepth;
        m_haveBacking = true;
        m_lastBacking = slot.backing;
        m_lastColorDepth = status.colorDepth;

        slot.backingChanged = backingChanged;
        if (backingChanged)
        {
            /* the old backings are purged at acquireCurrentFrame() after all of them read out */
            m_objectsFlushed = true;
            printf("icapturesession: backing changed at decoder buffer %llu: %ux%u -> %ux%u, "
                            "color depth %u -> %u\n",
                    (unsigned long long)bufferId,
                    oldBacking.width, oldBacking.height, status.width, status.height,
                    oldColorDepth, (unsigned)status.colorDepth);
        }
        purgeOldFramesLocked(purged);
        return OfferResult::Accepted;
    }

    mutable std::mutex m_mutex;
    BufferCapture m_capture;
    std::vector<ReleasedFrame> m_released; /* purged by purgeObsoleteFramesLocked(), not yet released by the MW */
    uint64_t m_droppedFrames{0}; /* frames purged by the STC before acquireCurrentFrame() took them */
    std::vector<Slot> m_slots;
    uint64_t m_nextToken{1};
    uint64_t m_nextSequence{1};

    bool m_poolCreated{false};
    bool m_connected{false};
    bool m_everConnected{false};
    bool m_fatal{false};
    bool m_unsupported{false};

    NEXUS_SimpleStcChannelHandle m_stcChannel{nullptr};
    NEXUS_SimpleStcChannelSettings m_stcSettings; /* settings before startBufferCapture(), restored by stopBufferCapture() */
    NEXUS_SimpleStcChannelHandle m_stcSettingsChannel{nullptr};
    bool m_stcSettingsSaved{false};
    bool m_everAcquired{false}; /* a frame was returned and no newer one has arrived */

    uint32_t m_admittedSlots{0};
    bool m_objectsFlushed{false};
    bool m_haveBacking{false};
    Size m_lastBacking{};
    unsigned m_lastColorDepth{0};
};

BrcmCaptureSession *asBrcmSession(void *session)
{
    /* Not a session of this library (e.g. a foreign pointer): never dereference it. */
    {
        std::lock_guard<std::mutex> lock(g_liveSessionsMutex);
        if (!session || std::find(g_liveSessions.begin(), g_liveSessions.end(), session) == g_liveSessions.end())
        {
            return nullptr;
        }
    }
    return static_cast<BrcmCaptureSession *>(static_cast<ICaptureSession *>(session));
}

} // namespace

/* Opaque link handed to the sink by RdkCaptureSinkBridge::connect(). */
struct RdkCaptureSinkLink
{
    BrcmCaptureSession *session;
    GstElement *sink;
};

namespace
{

BrcmCaptureSession *sessionOf(RdkCaptureSinkLink *link)
{
    return link ? link->session : nullptr;
}

RdkCaptureSinkLink *bridgeConnect(void *session, GstElement *sink)
{
    BrcmCaptureSession *s = asBrcmSession(session);
    if (!s || !s->connect())
    {
        return nullptr;
    }
    RdkCaptureSinkLink *link = new (std::nothrow) RdkCaptureSinkLink{s, sink};
    if (!link)
    {
        s->disconnect();
    }
    return link;
}

void bridgeDisconnect(RdkCaptureSinkLink *link)
{
    if (BrcmCaptureSession *s = sessionOf(link))
    {
        s->disconnect();
    }
    delete link;
}

/* The session owns its decoder buffers (buffer mode), so the sink requests none. */
uint32_t bridgeRetentionHeadroom(RdkCaptureSinkLink *)
{
    return 0;
}

/* The sink negotiates compressed video with the Nexus decoder; frames are exported from
 * the decoder, not negotiated as DMA_DRM caps, so there is nothing to validate here. */
gboolean bridgeConfigure(RdkCaptureSinkLink *, GstCaps *)
{
    return TRUE;
}

/* Frames come from the decoder output events, not from GstBuffers. */
void bridgeObserve(RdkCaptureSinkLink *, GstBuffer *)
{
}

void bridgePublish(RdkCaptureSinkLink *, GstBuffer *)
{
}

void bridgeInvalidateSelection(RdkCaptureSinkLink *link)
{
    if (BrcmCaptureSession *s = sessionOf(link))
    {
        s->flush();
    }
}

const RdkCaptureSinkBridge g_bridge = {
    RDK_CAPTURE_SINK_BRIDGE_VERSION,
    bridgeConnect,
    bridgeDisconnect,
    bridgeRetentionHeadroom,
    bridgeConfigure,
    bridgeObserve,
    bridgePublish,
    bridgeInvalidateSelection,
};

void extSetStcChannel(RdkCaptureSinkLink *link, void *stcChannel)
{
    if (BrcmCaptureSession *s = sessionOf(link))
    {
        s->setStcChannel(stcChannel);
    }
}

int extPrepareBufferCapture(RdkCaptureSinkLink *link, void *simpleVideoDecoder)
{
    BrcmCaptureSession *s = sessionOf(link);
    return (s && simpleVideoDecoder && s->prepareBufferCapture(simpleVideoDecoder)) ? 1 : 0;
}

int extStartBufferCapture(RdkCaptureSinkLink *link, void *simpleVideoDecoder)
{
    BrcmCaptureSession *s = sessionOf(link);
    return (s && simpleVideoDecoder && s->startBufferCapture(simpleVideoDecoder)) ? 1 : 0;
}

void extStopBufferCapture(RdkCaptureSinkLink *link)
{
    if (BrcmCaptureSession *s = sessionOf(link))
    {
        s->stopBufferCapture();
    }
}

const BrcmCaptureExt g_ext = {
    BRCM_CAPTURE_EXT_VERSION,
    extSetStcChannel,
    extPrepareBufferCapture,
    extStartBufferCapture,
    extStopBufferCapture,
};

} // namespace

extern "C" {

/* Vendor factory (spec section 8.1). MW owns the session and destroys it with delete. */
ICaptureSession *createCaptureSession()
{
    initDebugCategory();
    return new (std::nothrow) BrcmCaptureSession();
}

const RdkCaptureSinkBridge *rdkCaptureSinkBridgeV1(void)
{
    return &g_bridge;
}

const BrcmCaptureExt *rdkCaptureBroadcomExtV1(void)
{
    return &g_ext;
}

} // extern "C"
