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
 * capture_session_feeder: capture "server" for the SoC video frame capture spec.
 *
 * Plays a stream through playbin + westerossink with an ICaptureSession
 * attached through the rdk-capture-session property, acquires the current
 * frame, and forwards its description and dmabuf FDs (SCM_RIGHTS) to
 * capture_session_renderer over a unix socket.  A frame is released to the HAL only
 * after the client acknowledges it.
 *
 * NOTE: ICaptureSession is a C++ interface, so this file is C++. Build with the
 * Makefile in this directory (header from ../../src/ICaptureSession.h).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <signal.h>
#include <glib-unix.h>
#include <map>

#define EOS_STALL_US (4 * G_USEC_PER_SEC)
#include <gst/gst.h>

#include "ICaptureSession.h"
#include "capture_session_ipc.h"

using namespace rdkcapture;

#define ACQUIRE_POLL_MS 8

typedef struct _Feed {
    GstElement *playbin;
    GstElement *video_sink;
    GMainLoop *main_loop;

    /* options */
    const char *url;
    const char *lib_path;
    const char *socket_path;
    unsigned x, y, width, height;
    gint zorder;
    gboolean noaudio, use_playbin3, use_brcmsink;
    int loop_mode;              /* LOOP_NONE / LOOP_RESTART / LOOP_SEEK */
    double loop_seek_sec;       /* position -loop_seek returns to, in seconds (default 0) */
    gboolean rearm_stall;       /* set after a loop seek: forget the old last-frame time */
    guint requested_slots;      /* 0: use maximumSlots */

    /* capture */
    void *lib;
    ICaptureSession *session;
    CaptureCapabilities caps;
    CapturePool pool;
    guint slots;
    gboolean fatal;
    gboolean pending_flush;     /* latched until delivered to the client */
    guint64 seq;
    std::map<guint64, CapturedFrame *> held;   /* sent, awaiting client ack */

    /* ipc */
    int listen_fd;
    int client_fd;
    guint client_watch;
    guint acquire_timer;
} Feed;

static volatile gint g_stop_requested = 0;   /* set by the signal handler */

enum { LOOP_NONE = 0, LOOP_RESTART, LOOP_SEEK };

static Feed feed;

/* ------------------------------------------------------------------ */
/* frame lifetime                                                      */
/* ------------------------------------------------------------------ */

static void close_frame_fds(CapturedFrame *f)
{
    for (size_t i = 0; i < f->exportedDmaBufs.size(); i++) {
        if (f->exportedDmaBufs[i].fd >= 0) close(f->exportedDmaBufs[i].fd);
        f->exportedDmaBufs[i].fd = -1;
    }
}

static void release_and_free(Feed *s, CapturedFrame *f)
{
    close_frame_fds(f);
    CaptureStatus st = s->session->releaseFrame(*f);
    if (st != CaptureStatus::OK)
        g_printerr("releaseFrame failed: %u\n", (unsigned)st);
    delete f;
}

static void release_all_held(Feed *s)
{
    for (std::map<guint64, CapturedFrame *>::iterator it = s->held.begin(); it != s->held.end(); ++it)
        release_and_free(s, it->second);
    s->held.clear();
}

/* ------------------------------------------------------------------ */
/* IPC                                                                 */
/* ------------------------------------------------------------------ */

static void drop_client(Feed *s)
{
    if (s->client_watch) { g_source_remove(s->client_watch); s->client_watch = 0; }
    if (s->client_fd >= 0) { close(s->client_fd); s->client_fd = -1; }
    release_all_held(s);
}

static gboolean client_cb(GIOChannel *, GIOCondition cond, gpointer data)
{
    Feed *s = (Feed *)data;
    CaptureSessionAckMsg ack;
    ssize_t n = recv(s->client_fd, &ack, sizeof(ack), MSG_DONTWAIT);

    if (n == (ssize_t)sizeof(ack) && ack.magic == CAPTURE_SESSION_MAGIC) {
        std::map<guint64, CapturedFrame *>::iterator it = s->held.find(ack.seq);
        if (it != s->held.end()) {
            release_and_free(s, it->second);
            s->held.erase(it);
        }
        return TRUE;
    }
    if (n < 0 && (errno == EAGAIN || errno == EINTR) && !(cond & (G_IO_HUP | G_IO_ERR)))
        return TRUE;

    g_print("client disconnected\n");
    s->client_watch = 0;        /* returning FALSE removes the source */
    drop_client(s);
    return FALSE;
}

static gboolean accept_cb(GIOChannel *, GIOCondition, gpointer data)
{
    Feed *s = (Feed *)data;
    int fd = accept4(s->listen_fd, NULL, NULL, SOCK_CLOEXEC);
    if (fd < 0) return TRUE;

    if (s->client_fd >= 0) drop_client(s);      /* one client at a time */
    s->client_fd = fd;
    /* A new client has an empty cache; make that explicit. */
    s->pending_flush = TRUE;

    GIOChannel *ch = g_io_channel_unix_new(fd);
    s->client_watch = g_io_add_watch(ch, (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR), client_cb, s);
    g_io_channel_unref(ch);
    g_print("client connected\n");
    return TRUE;
}

static gboolean setup_socket(Feed *s)
{
    struct sockaddr_un addr;
    s->listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (s->listen_fd < 0) { perror("socket"); return FALSE; }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", s->socket_path);
    unlink(s->socket_path);
    if (bind(s->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(s->listen_fd, 1) < 0) {
        perror("bind/listen");
        return FALSE;
    }
    GIOChannel *ch = g_io_channel_unix_new(s->listen_fd);
    g_io_add_watch(ch, G_IO_IN, accept_cb, s);
    g_io_channel_unref(ch);
    return TRUE;
}

/* Identity of the complete import description (ordered object ids / plane
 * mapping, format, modifier, geometry, colour).  Same value => the client may
 * reuse its EGL image; the colour fields are part of it per spec 6.2. */
static guint64 fnv(guint64 h, guint64 v)
{
    for (int i = 0; i < 8; i++) { h ^= (v >> (i * 8)) & 0xff; h *= 1099511628211ull; }
    return h;
}

static guint64 compute_buffer_id(const CapturedFrame *f)
{
    guint64 h = 1469598103934665603ull;
    h = fnv(h, f->capabilities.drmFormat);
    h = fnv(h, f->capabilities.drmModifier);
    h = fnv(h, ((guint64)f->backingSize.width << 32) | f->backingSize.height);
    for (size_t i = 0; i < f->planes.size(); i++) {
        h = fnv(h, f->planes[i].planeIndex);
        h = fnv(h, f->planes[i].dmaBufObjectId);
        h = fnv(h, f->planes[i].offsetBytes);
        h = fnv(h, f->planes[i].strideBytes);
    }
    return h;
}

/* Spec 5.1 range / visible-region validation, overflow-safe. */
static gboolean validate_frame(const CapturedFrame *f)
{
#define BAD(...) do { g_printerr("validate_frame: " __VA_ARGS__); g_printerr("\n"); return FALSE; } while (0)
    if (f->planes.empty() || f->planes.size() > CAPTURE_SESSION_MAX_PLANES) BAD("bad plane count %zu", f->planes.size());
    if (f->exportedDmaBufs.empty() || f->exportedDmaBufs.size() > CAPTURE_SESSION_MAX_FDS)
        BAD("bad exported fd count %zu", f->exportedDmaBufs.size());

    for (size_t i = 0; i < f->planes.size(); i++) {
        const FramePlane &p = f->planes[i];
        gboolean found = FALSE;
        for (size_t j = 0; j < f->dmaBufObjects.size(); j++) {
            const DmaBufObject &o = f->dmaBufObjects[j];
            if (o.objectId == p.dmaBufObjectId) {
                if (p.offsetBytes > o.sizeBytes || p.lengthBytes > o.sizeBytes - p.offsetBytes)
                    BAD("plane %zu range %llu+%llu exceeds object %llu size %llu", i,
                        (unsigned long long)p.offsetBytes, (unsigned long long)p.lengthBytes,
                        (unsigned long long)o.objectId, (unsigned long long)o.sizeBytes);
                found = TRUE;
            }
        }
        if (!found || p.dmaBufObjectId == 0) BAD("plane %zu references unknown object %llu", i, (unsigned long long)p.dmaBufObjectId);
        gboolean exported = FALSE;
        for (size_t j = 0; j < f->exportedDmaBufs.size(); j++)
            if (f->exportedDmaBufs[j].dmaBufObjectId == p.dmaBufObjectId) exported = TRUE;
        if (!exported) BAD("plane %zu object %llu has no exported fd", i, (unsigned long long)p.dmaBufObjectId);
    }
    const Rectangle &v = f->visibleRegion;
    const Size &b = f->backingSize;
    if (v.x > b.width || v.size.width > b.width - v.x ||
        v.y > b.height || v.size.height > b.height - v.y)
        BAD("visible (%u,%u %ux%u) outside backing %ux%u", v.x, v.y, v.size.width, v.size.height, b.width, b.height);
    return TRUE;
#undef BAD
}

/* Returns TRUE if the message was delivered. Closes the local FD copies. */
static gboolean send_frame(Feed *s, CapturedFrame *f, guint64 seq)
{
    CaptureSessionFrameMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic = CAPTURE_SESSION_MAGIC;
    msg.slotCount = s->slots;
    msg.seq = seq;
    msg.bufferId = compute_buffer_id(f);
    msg.flushed = s->pending_flush ? 1 : 0;      /* HAL flag already folded in */
    msg.nfds = (uint32_t)f->exportedDmaBufs.size();
    msg.nplanes = (uint32_t)f->planes.size();
    msg.drmFormat = f->capabilities.drmFormat;
    msg.drmModifier = f->capabilities.drmModifier;
    msg.backingWidth = f->backingSize.width;
    msg.backingHeight = f->backingSize.height;
    msg.visX = f->visibleRegion.x;
    msg.visY = f->visibleRegion.y;
    msg.visWidth = f->visibleRegion.size.width;
    msg.visHeight = f->visibleRegion.size.height;
    msg.ptsNs = f->presentationTimeNs;
    msg.matrix = 0;
    msg.range = 0;
    msg.hSiting = 0;
    msg.vSiting = 0;

    int fds[CAPTURE_SESSION_MAX_FDS];
    for (size_t i = 0; i < f->exportedDmaBufs.size(); i++) fds[i] = f->exportedDmaBufs[i].fd;

    for (size_t i = 0; i < f->planes.size(); i++) {
        msg.planes[i].planeIndex = f->planes[i].planeIndex;
        msg.planes[i].offsetBytes = (uint32_t)f->planes[i].offsetBytes;
        msg.planes[i].strideBytes = f->planes[i].strideBytes;
        for (size_t j = 0; j < f->exportedDmaBufs.size(); j++)
            if (f->exportedDmaBufs[j].dmaBufObjectId == f->planes[i].dmaBufObjectId)
                msg.planes[i].fdIndex = (uint32_t)j;
    }

    union { char buf[CMSG_SPACE(sizeof(int) * CAPTURE_SESSION_MAX_FDS)]; struct cmsghdr align; } u;
    memset(&u, 0, sizeof(u));
    struct iovec iov = { &msg, sizeof(msg) };
    struct msghdr mh;
    memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    mh.msg_control = u.buf;
    mh.msg_controllen = CMSG_SPACE(sizeof(int) * msg.nfds);
    struct cmsghdr *c = CMSG_FIRSTHDR(&mh);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(int) * msg.nfds);
    memcpy(CMSG_DATA(c), fds, sizeof(int) * msg.nfds);

    ssize_t n = sendmsg(s->client_fd, &mh, MSG_NOSIGNAL);
    close_frame_fds(f);         /* receiver owns its own copies now */
    return n == (ssize_t)sizeof(msg);
}

/* ------------------------------------------------------------------ */
/* acquisition                                                         */
/* ------------------------------------------------------------------ */

/* End of stream detected (bus EOS or the stall fallback).  Either leave the main
 * loop (main() then ends or restarts the pipeline) or, for -loop_seek, flush-seek
 * the running pipeline back to the -loop_seek position (default the start). */
static void handle_end_of_stream(Feed *s)
{
    if (s->loop_mode == LOOP_SEEK && !g_atomic_int_get(&g_stop_requested)) {
        g_print("loop_seek: seeking to %.3f s\n", s->loop_seek_sec);
        s->rearm_stall = TRUE;
        if (!gst_element_seek_simple(s->playbin, GST_FORMAT_TIME,
                                     (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                                     (gint64)(s->loop_seek_sec * GST_SECOND))) {
            g_printerr("loop_seek: seek failed, stopping\n");
            if (s->main_loop) g_main_loop_quit(s->main_loop);
        }
        return;
    }
    if (s->main_loop) g_main_loop_quit(s->main_loop);
}

static gboolean acquire_cb(gpointer data)
{
    Feed *s = (Feed *)data;

    static guint64 polls, st_count[8], full_skips, no_client, sent;
    static gint64 last_report;
    gint64 now = g_get_monotonic_time();
    if (!last_report) last_report = now;
    if (now - last_report >= 2000000) {
        g_print("feed stats: polls=%" G_GUINT64_FORMAT " noClientSkips=%" G_GUINT64_FORMAT " fullSkips=%" G_GUINT64_FORMAT
                " OK=%" G_GUINT64_FORMAT " NO_NEW_FRAME=%" G_GUINT64_FORMAT " NO_FRAME=%" G_GUINT64_FORMAT
                " NO_RESOURCES=%" G_GUINT64_FORMAT " other=%" G_GUINT64_FORMAT " sent=%" G_GUINT64_FORMAT
                " held=%zu client=%s fatal=%d\n",
                polls, no_client, full_skips, st_count[0], st_count[1], st_count[5], st_count[4],
                st_count[2] + st_count[3] + st_count[6], sent, s->held.size(),
                s->client_fd >= 0 ? "yes" : "no", (int)s->fatal);
        last_report = now;
    }
    if (s->fatal) return TRUE;
    if (s->client_fd < 0) { no_client++; return TRUE; }
    if (s->held.size() >= s->slots) { full_skips++; return TRUE; }  /* full: never stall playback */

    /* End-of-stream fallback.  In capture mode the sink may never post EOS (its
     * own check looks at the decoder fifo / last render pts), so infer it: no frame
     * for EOS_STALL_US with nothing held after playback has produced frames.
     * Position/duration are only printed: this pipeline's position query does not
     * reliably reach the duration. */
    static gint64 last_ok_us;
    if (sent == 0 || s->rearm_stall) { last_ok_us = 0; s->rearm_stall = FALSE; }
    if (sent > 0 && last_ok_us && s->held.empty() && now - last_ok_us > EOS_STALL_US) {
        gint64 pos = -1, dur = -1;
        gboolean have_pos = gst_element_query_position(s->playbin, GST_FORMAT_TIME, &pos);
        gboolean have_dur = gst_element_query_duration(s->playbin, GST_FORMAT_TIME, &dur);
        g_print("no new frame for %d s (position %" G_GINT64_FORMAT " ms, duration %" G_GINT64_FORMAT
                " ms): treating as end of stream, sink did not post EOS\n",
                (int)(EOS_STALL_US / 1000000), have_pos ? pos / GST_MSECOND : -1, have_dur ? dur / GST_MSECOND : -1);
        last_ok_us = 0;
        if (s->loop_mode != LOOP_SEEK) sent = 0;    /* re-arm for the next run */
        handle_end_of_stream(s);
        return TRUE;
    }

    polls++;
    CapturedFrame *f = new CapturedFrame();         /* must be empty (7.3) */
    CaptureStatus st = s->session->acquireCurrentFrame(*f);
    st_count[(unsigned)st < 8 ? (unsigned)st : 7]++;
    if (st == CaptureStatus::OK) last_ok_us = now;

    if (st != CaptureStatus::OK) {
        delete f;                                   /* output unchanged, no obligation */
        if (st == CaptureStatus::FATAL_ERROR) {
            g_printerr("acquireCurrentFrame: FATAL_ERROR, capture disabled for this session\n");
            s->fatal = TRUE;
        } else if (st != CaptureStatus::NO_NEW_FRAME && st != CaptureStatus::NO_FRAME &&
                   st != CaptureStatus::NO_RESOURCES) {
            g_printerr("acquireCurrentFrame: status %u\n", (unsigned)st);
        }
        return TRUE;
    }

    /* Latch: stays set until a send succeeds, even if later HAL flags are false. */
    if (f->dmaBufObjectsFlushed) s->pending_flush = TRUE;

    if (!validate_frame(f)) {
        g_printerr("invalid frame description from HAL, dropping\n");
        release_and_free(s, f);
        return TRUE;
    }

    guint64 seq = ++s->seq;
    if (send_frame(s, f, seq)) {
        sent++;
        if (sent <= 5)
            g_print("sent seq=%" G_GUINT64_FORMAT " bufferId=%016" G_GINT64_MODIFIER "x fmt=0x%08x mod=0x%" G_GINT64_MODIFIER "x "
                    "backing=%ux%u vis=(%u,%u %ux%u) nfds=%zu nplanes=%zu flushed=%d\n",
                    seq, compute_buffer_id(f), f->capabilities.drmFormat, f->capabilities.drmModifier,
                    f->backingSize.width, f->backingSize.height, f->visibleRegion.x, f->visibleRegion.y,
                    f->visibleRegion.size.width, f->visibleRegion.size.height,
                    f->exportedDmaBufs.size(), f->planes.size(), (int)f->dmaBufObjectsFlushed);
        s->pending_flush = FALSE;
        s->held[seq] = f;       /* lock kept until the client acks */
    } else {
        g_printerr("send failed, releasing frame (pending flush kept)\n");
        release_and_free(s, f);
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* pipeline                                                            */
/* ------------------------------------------------------------------ */

/* Ctrl+C / kill: leave the main loop so main() does the orderly teardown.
 * Dying with the decoder still running makes the Nexus driver clean it up in
 * the kernel at process exit, which can assert and take the whole box down. */

static gboolean signal_cb(gpointer data)
{
    Feed *s = (Feed *)data;
    g_print("signal received, shutting down\n");
    g_atomic_int_set(&g_stop_requested, 1);
    if (s->main_loop) g_main_loop_quit(s->main_loop);
    return G_SOURCE_CONTINUE;   /* removed in main() */
}

/* Same elements pbin_play.c sets "enable-svp" on; here it is forced to FALSE so
 * the decoded frames stay CPU/GPU-importable for capture. */
static void on_element_added(GstBin *, GstBin *, GstElement *element, gpointer)
{
    const gchar *name = GST_ELEMENT_NAME(element);
    if (g_strrstr(name, "brcmvideodecoder") || g_strrstr(name, "brcmaudiodecoder") ||
        g_strrstr(name, "brcmvidfilter") || g_strrstr(name, "brcmaudfilter")) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), "enable-svp")) {
            g_object_set(element, "enable-svp", FALSE, NULL);
            g_print("%s: enable-svp=FALSE\n", name);
        }
    }
}

static gboolean bus_cb(GstBus *, GstMessage *message, gpointer data)
{
    Feed *s = (Feed *)data;
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS:
            g_print("bus: EOS from %s\n", GST_MESSAGE_SRC(message) ? GST_OBJECT_NAME(GST_MESSAGE_SRC(message)) : "?");
            handle_end_of_stream(s);
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = NULL; gchar *dbg = NULL;
            gst_message_parse_error(message, &err, &dbg);
            g_printerr("ERROR: %s (%s)\n", err->message, dbg ? dbg : "");
            g_error_free(err); g_free(dbg);
            g_main_loop_quit(s->main_loop);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(s->playbin)) {
                GstState o, n, p;
                gst_message_parse_state_changed(message, &o, &n, &p);
                g_print("bus: playbin %s -> %s\n", gst_element_state_get_name(o), gst_element_state_get_name(n));
            }
            break;
        case GST_MESSAGE_WARNING: {
            GError *err = NULL; gchar *dbg = NULL;
            gst_message_parse_warning(message, &err, &dbg);
            g_printerr("WARNING from %s: %s (%s)\n", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)), err->message, dbg ? dbg : "");
            g_error_free(err); g_free(dbg);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

/* MW owns the session: destroy with delete via the virtual destructor (spec 4.6).
 * The library stays loaded until this returns. */
static void destroy_session(Feed *s)
{
    if (!s->session) return;
    delete s->session;
    s->session = NULL;
}

#define CAPTURE_LIB_DIR "/usr/lib/gstreamer-1.0/"
static gboolean load_session(Feed *s)
{
    /* A bare library name is looked up in the GStreamer plugin dir first. */
    if (s->lib_path[0] != '/') {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s%s", CAPTURE_LIB_DIR, s->lib_path);
        if (access(full, R_OK) == 0) s->lib = dlopen(full, RTLD_NOW);
    }
    if (!s->lib) s->lib = dlopen(s->lib_path, RTLD_NOW);
    if (!s->lib) { g_printerr("dlopen(%s): %s\n", s->lib_path, dlerror()); return FALSE; }
    CreateCaptureSessionFn create = (CreateCaptureSessionFn)dlsym(s->lib, RDK_CAPTURE_CREATE_SESSION_SYMBOL);
    if (!create) { g_printerr("createCaptureSession not found\n"); return FALSE; }
    s->session = create();
    if (!s->session) { g_printerr("createCaptureSession returned NULL\n"); return FALSE; }
    return TRUE;
}

/* Spec 4.5: sink gets the borrowed session, reaches READY, then query + createPool once. */
static gboolean setup_capture(Feed *s)
{
    g_object_set(s->video_sink, "secure-video", FALSE, NULL);

    /* g_object_set() on a missing property only logs a warning; fail instead. */
    if (!g_object_class_find_property(G_OBJECT_GET_CLASS(s->video_sink), "rdk-capture-session")) {
        g_printerr("westerossink has no 'rdk-capture-session' property: "
                   "the installed plugin lacks capture support (spec 4.2/4.3)\n");
        return FALSE;
    }
    g_object_set(s->video_sink, "rdk-capture-session", (gpointer)s->session, NULL);

    /* The sink has no parent yet, so give it a private bus to collect the
     * reason a failed NULL->READY would otherwise swallow. */
    GstBus *tmp_bus = gst_bus_new();
    gst_element_set_bus(s->video_sink, tmp_bus);
    GstStateChangeReturn sret = gst_element_set_state(s->video_sink, GST_STATE_READY);
    GstMessage *m;
    while ((m = gst_bus_pop_filtered(tmp_bus, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING))) != NULL) {
        GError *err = NULL; gchar *dbg = NULL;
        if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_ERROR) gst_message_parse_error(m, &err, &dbg);
        else gst_message_parse_warning(m, &err, &dbg);
        g_printerr("sink %s: %s (%s)\n", GST_MESSAGE_TYPE(m) == GST_MESSAGE_ERROR ? "ERROR" : "WARNING",
                   err->message, dbg ? dbg : "");
        g_error_free(err); g_free(dbg);
        gst_message_unref(m);
    }
    gst_element_set_bus(s->video_sink, NULL);   /* playbin installs its own on add */
    gst_object_unref(tmp_bus);
    if (sret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("westerossink failed to reach READY with capture session\n");
        return FALSE;
    }

    if (s->session->getCapabilities(s->caps) != CaptureStatus::OK) {
        g_printerr("getCapabilities failed\n");
        return FALSE;
    }
    g_print("caps: drmFormat=0x%08x modifier=0x%llx max=%ux%u slots=[%u,%u]\n",
            s->caps.drmFormat, (unsigned long long)s->caps.drmModifier,
            s->caps.maximumContentSize.width, s->caps.maximumContentSize.height,
            s->caps.minimumSlots, s->caps.maximumSlots);

    s->slots = s->requested_slots ? s->requested_slots : s->caps.maximumSlots;
    if (s->slots < s->caps.minimumSlots || s->slots > s->caps.maximumSlots) {
        g_printerr("slot count %u outside [%u,%u]\n", s->slots, s->caps.minimumSlots, s->caps.maximumSlots);
        return FALSE;
    }
    if (s->session->createPool(s->slots, s->pool) != CaptureStatus::OK) {
        g_printerr("createPool(%u) failed\n", s->slots);
        return FALSE;
    }
    g_print("pool created: %u slots, backing %ux%u\n", s->slots,
            s->pool.backingSize.width, s->pool.backingSize.height);
    return TRUE;
}

static void print_usage(void)
{
    printf(
    "Usage: capture_session_feeder OPTIONS stream_uri\n"
    "\n"
    "OPTIONS:\n"
    "  --help or -h                 help\n"
    "  (vendor capture library: set RDK_CAPTURE_LIBRARY, default libbroadcom_capture.so)\n"
    "  -socket PATH                 unix socket (default %s)\n"
    "  -slots N                     requested pool capacity (default: maximumSlots)\n"
    "  -rect x,y,width,height       video window rect\n"
    "  -zorder NUM                  zorder\n"
    "  -noaudio                     don't play audio\n"
    "  -playbin3                    use playbin3\n"
    "  -brcmsink                    use brcmvideosink instead of westerossink (no capture)\n"
    "  -loop_restart                loop by restarting the whole pipeline and capture session at EOS\n"
    "  -loop_seek [SECONDS]         loop by flush-seeking the running pipeline to SECONDS (default 0) at EOS\n", CAPTURE_SESSION_DEFAULT_SOCKET);
}

int main(int argc, char *argv[])
{
    Feed *s = &feed;
    int curarg = 1;
    int ret = 0;
    GstBus *bus = NULL;
    guint bus_watch = 0;
    guint sig_int = 0, sig_term = 0, sig_hup = 0;

    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);   /* keep the log complete if we crash while piped to tee */
    s->lib_path = getenv("RDK_CAPTURE_LIBRARY");
    if (!s->lib_path || !*s->lib_path) s->lib_path = "libbroadcom_capture.so";
    s->socket_path = CAPTURE_SESSION_DEFAULT_SOCKET;
    s->listen_fd = s->client_fd = -1;

    while (argc > curarg) {
        if (!strcmp(argv[curarg], "-h") || !strcmp(argv[curarg], "--help")) { print_usage(); return 0; }
        else if (!strcmp(argv[curarg], "-socket") && argc > curarg + 1) s->socket_path = argv[++curarg];
        else if (!strcmp(argv[curarg], "-slots") && argc > curarg + 1) s->requested_slots = atoi(argv[++curarg]);
        else if (!strcmp(argv[curarg], "-zorder") && argc > curarg + 1) s->zorder = atoi(argv[++curarg]);
        else if (!strcmp(argv[curarg], "-noaudio")) s->noaudio = TRUE;
        else if (!strcmp(argv[curarg], "-playbin3")) s->use_playbin3 = TRUE;
        else if (!strcmp(argv[curarg], "-brcmsink")) s->use_brcmsink = TRUE;
        else if (!strcmp(argv[curarg], "-loop_restart")) s->loop_mode = LOOP_RESTART;
        else if (!strcmp(argv[curarg], "-loop_seek")) {
            s->loop_mode = LOOP_SEEK;
            /* optional position: only a complete non-negative number is taken, so a URL
             * or another option following -loop_seek is left alone */
            if (argc > curarg + 1) {
                char *end = NULL;
                double sec = strtod(argv[curarg + 1], &end);
                if (end != argv[curarg + 1] && *end == '\0' && sec >= 0) {
                    s->loop_seek_sec = sec;
                    curarg++;
                }
            }
        }
        else if (!strcmp(argv[curarg], "-rect") && argc > curarg + 1) {
            unsigned x, y, w, h;
            if (sscanf(argv[++curarg], "%u,%u,%u,%u", &x, &y, &w, &h) == 4) {
                s->x = x; s->y = y; s->width = w; s->height = h;
            }
        }
        else if (!s->url) s->url = argv[curarg];
        else { print_usage(); return 0; }
        curarg++;
    }
    if (!s->url) { print_usage(); return 0; }

    gst_init(&argc, &argv);

    if (!s->use_brcmsink) {
        if (!load_session(s)) return -1;
        if (!setup_socket(s)) return -1;
    }

_restart:
    s->playbin = gst_element_factory_make(s->use_playbin3 ? "playbin3" : "playbin", "playbin");
    s->video_sink = gst_element_factory_make(s->use_brcmsink ? "brcmvideosink" : "westerossink", "videosink");
    if (!s->playbin || !s->video_sink) { g_printerr("Not all elements could be created.\n"); return -1; }

    g_object_set(s->playbin, "uri", s->url, NULL);
    g_object_set(s->playbin, "flags", 0x1 | 0x2 | 0x20 | 0x40, NULL);   /* video|audio|native-* */
    g_signal_connect(s->playbin, "deep-element-added", G_CALLBACK(on_element_added), NULL);
    g_object_set(s->playbin, "video-sink", s->video_sink, NULL);
    if (s->noaudio) {
        g_object_set(s->playbin, "flags", 0x1 | 0x20, NULL);
    }

    if (s->width && s->height) {
        char str[40];
        snprintf(str, sizeof(str), "%u,%u,%u,%u", s->x, s->y, s->width, s->height);
        g_object_set(s->video_sink, s->use_brcmsink ? "window_set" : "window-set", str, NULL);
    }
    if (s->use_brcmsink)
        g_object_set(s->video_sink, "zorder", s->zorder, NULL);
    else
        g_object_set(s->video_sink, "zorder", (gfloat)s->zorder, NULL);

    if (!s->use_brcmsink && !setup_capture(s)) {
        /* Spec 4.2: roll back to NULL; the session is retired. */
        gst_element_set_state(s->playbin, GST_STATE_NULL);
        gst_element_set_state(s->video_sink, GST_STATE_NULL);
        ret = -1;
        goto cleanup;
    }

    s->main_loop = g_main_loop_new(NULL, FALSE);
    sig_int = g_unix_signal_add(SIGINT, signal_cb, s);
    sig_term = g_unix_signal_add(SIGTERM, signal_cb, s);
    sig_hup = g_unix_signal_add(SIGHUP, signal_cb, s);
    bus = gst_element_get_bus(s->playbin);
    bus_watch = gst_bus_add_watch(bus, (GstBusFunc)bus_cb, s);

    if (gst_element_set_state(s->playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        ret = -1;
    } else {
        if (!s->use_brcmsink)
            s->acquire_timer = g_timeout_add(ACQUIRE_POLL_MS, acquire_cb, s);
        g_main_loop_run(s->main_loop);
        if (s->acquire_timer) { g_source_remove(s->acquire_timer); s->acquire_timer = 0; }
    }

    g_source_remove(bus_watch);
    g_source_remove(sig_int); g_source_remove(sig_term); g_source_remove(sig_hup);
    gst_object_unref(bus);
    g_main_loop_unref(s->main_loop);

    /* Spec 4.6: NULL (disconnects capture) first, only then final releases. */
    g_print("teardown: playbin -> NULL\n");
    gst_element_set_state(s->playbin, GST_STATE_NULL);
    g_print("teardown: playbin is NULL, unref\n");
    gst_object_unref(s->playbin);

    if (s->loop_mode == LOOP_RESTART && ret == 0 && !g_atomic_int_get(&g_stop_requested)) {
        /* A session is never reused (spec 4.4): retire and create a fresh one. */
        g_print("teardown: releasing %zu held frame(s)\n", s->held.size());
        release_all_held(s);
        g_print("teardown: destroying session\n");
        destroy_session(s);
        g_print("teardown: restarting pipeline\n");
        s->fatal = FALSE;
        s->pending_flush = TRUE;
        s->pool = CapturePool();
        if (!s->use_brcmsink && !(s->session = ((CreateCaptureSessionFn)dlsym(s->lib, RDK_CAPTURE_CREATE_SESSION_SYMBOL))())) return -1;
        goto _restart;
    }

cleanup:
    drop_client(s);
    destroy_session(s);   /* after all releases */
    if (s->lib) dlclose(s->lib);                                /* only after deletion */
    if (s->listen_fd >= 0) { close(s->listen_fd); unlink(s->socket_path); }
    return ret;
}
