#include "sink_backend_mock.h"
#include "westeros-sink.h"
#include <string.h>

static SinkBackendMockState state;

void sink_backend_mock_reset(void)
{
    memset(&state, 0, sizeof(state));
    state.acceptCaps = true;
}

SinkBackendMockState sink_backend_mock_state(void) { return state; }
void sink_backend_mock_set_accept_caps(bool accept) { state.acceptCaps = accept; }
void sink_backend_mock_set_handle_queries(bool handle) { state.handleQueries = handle; }

void gst_westeros_sink_soc_class_init(GstWesterosSinkClass *klass) { klass->canUseResMgr = 0; }
gboolean gst_westeros_sink_soc_init(GstWesterosSink *sink)
{ (void)sink; ++state.initCalls; return TRUE; }
void gst_westeros_sink_soc_term(GstWesterosSink *sink)
{ (void)sink; ++state.termCalls; }
void gst_westeros_sink_soc_set_property(GObject *o, guint id, const GValue *v, GParamSpec *p)
{ (void)o; (void)id; (void)v; (void)p; }
void gst_westeros_sink_soc_get_property(GObject *o, guint id, GValue *v, GParamSpec *p)
{ (void)o; (void)id; (void)v; (void)p; }

#define STATE_TRANSITION(name) \
gboolean name(GstWesterosSink *sink, gboolean *passToDefault) \
{ (void)sink; ++state.stateTransitionCalls; *passToDefault = FALSE; return TRUE; }
STATE_TRANSITION(gst_westeros_sink_soc_null_to_ready)
STATE_TRANSITION(gst_westeros_sink_soc_ready_to_paused)
STATE_TRANSITION(gst_westeros_sink_soc_paused_to_playing)
STATE_TRANSITION(gst_westeros_sink_soc_playing_to_paused)
STATE_TRANSITION(gst_westeros_sink_soc_paused_to_ready)
STATE_TRANSITION(gst_westeros_sink_soc_ready_to_null)

void gst_westeros_sink_soc_registryHandleGlobal(GstWesterosSink *s, struct wl_registry *r, uint32_t id, const char *i, uint32_t v)
{ (void)s; (void)r; (void)id; (void)i; (void)v; }
void gst_westeros_sink_soc_registryHandleGlobalRemove(GstWesterosSink *s, struct wl_registry *r, uint32_t n)
{ (void)s; (void)r; (void)n; }
gboolean gst_westeros_sink_soc_accept_caps(GstWesterosSink *s, GstCaps *c)
{ (void)s; (void)c; ++state.acceptCapsCalls; return state.acceptCaps; }
void gst_westeros_sink_soc_set_startPTS(GstWesterosSink *s, gint64 pts)
{ (void)s; ++state.setStartPtsCalls; state.lastStartPts = pts; }
void gst_westeros_sink_soc_render(GstWesterosSink *s, GstBuffer *b)
{ (void)s; (void)b; ++state.renderCalls; }
void gst_westeros_sink_soc_flush(GstWesterosSink *s) { (void)s; ++state.flushCalls; }
gboolean gst_westeros_sink_soc_start_video(GstWesterosSink *s)
{ (void)s; ++state.startVideoCalls; return TRUE; }
void gst_westeros_sink_soc_eos_event(GstWesterosSink *s) { (void)s; ++state.eosCalls; }
void gst_westeros_sink_soc_set_video_path(GstWesterosSink *s, bool gfx) { (void)s; (void)gfx; }
void gst_westeros_sink_soc_update_video_position(GstWesterosSink *s) { (void)s; }
gboolean gst_westeros_sink_soc_query(GstWesterosSink *s, GstQuery *q)
{ (void)s; (void)q; ++state.queryCalls; return state.handleQueries; }