/*
 * L1 Test Suite for westeros-sink.c + westeros-sink-sw.c
 */

#include <gtest/gtest.h>
#include <glib.h>
#include <gst/gst.h>
#include <cstring>

extern "C" {
#include "westeros-sink.h"
#include "westeros-sink-sw.h"

extern GType gst_westeros_sink_get_type(void);

extern void gst_westeros_sink_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
extern void gst_westeros_sink_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
extern gboolean gst_westeros_sink_query(GstElement *element, GstQuery *query);
extern gboolean gst_westeros_sink_send_event(GstElement *element, GstEvent *event);
extern gboolean gst_westeros_sink_start(GstBaseSink *base_sink);
extern gboolean gst_westeros_sink_stop(GstBaseSink *base_sink);
extern gboolean gst_westeros_sink_unlock(GstBaseSink *base_sink);
extern gboolean gst_westeros_sink_unlock_stop(GstBaseSink *base_sink);
extern GstFlowReturn gst_westeros_sink_render(GstBaseSink *base_sink, GstBuffer *buffer);
extern GstFlowReturn gst_westeros_sink_preroll(GstBaseSink *base_sink, GstBuffer *buffer);
extern gboolean gst_westeros_sink_backend_null_to_ready(GstWesterosSink *sink, gboolean *passToDefault);
extern gboolean gst_westeros_sink_backend_ready_to_paused(GstWesterosSink *sink, gboolean *passToDefault);
extern gboolean gst_westeros_sink_backend_paused_to_playing(GstWesterosSink *sink, gboolean *passToDefault);
extern gboolean gst_westeros_sink_backend_playing_to_paused(GstWesterosSink *sink, gboolean *passToDefault);
extern gboolean gst_westeros_sink_backend_paused_to_ready(GstWesterosSink *sink, gboolean *passToDefault);
extern gboolean gst_westeros_sink_backend_ready_to_null(GstWesterosSink *sink, gboolean *passToDefault);
}

class WesterosSinkMainTest : public ::testing::Test {
protected:
    GstWesterosSink sink;
    GValue value_string, value_int, value_uint, value_float, value_boolean;
    GParamSpec pspec;
    gboolean values_initialized;
    
    static void SetUpTestSuite() {
        // Initialize GStreamer once for all tests
        if (!gst_is_initialized()) {
            gst_init(nullptr, nullptr);
        }
    }
    
    void SetUp() override {
        // Ensure type is registered
        gst_westeros_sink_get_type();
        
        std::memset(&sink, 0, sizeof(sink));
        std::memset(&pspec, 0, sizeof(pspec));
        values_initialized = FALSE;
        
        // Initialize GValues
        std::memset(&value_string, 0, sizeof(value_string));
        g_value_init(&value_string, G_TYPE_STRING);
        g_value_set_string(&value_string, "0:0:1920:1080");
        
        std::memset(&value_int, 0, sizeof(value_int));
        g_value_init(&value_int, G_TYPE_INT);
        g_value_set_int(&value_int, 0);
        
        std::memset(&value_uint, 0, sizeof(value_uint));
        g_value_init(&value_uint, G_TYPE_UINT);
        g_value_set_uint(&value_uint, 0);
        
        std::memset(&value_float, 0, sizeof(value_float));
        g_value_init(&value_float, G_TYPE_FLOAT);
        g_value_set_float(&value_float, 0.5f);
        
        std::memset(&value_boolean, 0, sizeof(value_boolean));
        g_value_init(&value_boolean, G_TYPE_BOOLEAN);
        g_value_set_boolean(&value_boolean, FALSE);
        
        values_initialized = TRUE;
    }
    
    void TearDown() override {
        if (values_initialized) {
            if (G_IS_VALUE(&value_string)) {
                g_value_unset(&value_string);
            }
            if (G_IS_VALUE(&value_int)) {
                g_value_unset(&value_int);
            }
            if (G_IS_VALUE(&value_uint)) {
                g_value_unset(&value_uint);
            }
            if (G_IS_VALUE(&value_float)) {
                g_value_unset(&value_float);
            }
            if (G_IS_VALUE(&value_boolean)) {
                g_value_unset(&value_boolean);
            }
            values_initialized = FALSE;
        }
    }
};

// ============================================================================
// SOFTWARE DECODER TESTS (westeros-sink-sw.c)
// ============================================================================

TEST_F(WesterosSinkMainTest, SWProcessCapsWithValidCaps) {
    GstCaps *caps = gst_caps_new_simple("video/x-h264",
                                         "width", G_TYPE_INT, 1920,
                                         "height", G_TYPE_INT, 1080,
                                         "framerate", GST_TYPE_FRACTION, 30, 1,
                                         NULL);
    ASSERT_NE(nullptr, caps);
    wstsw_process_caps(&sink, caps);
    gst_caps_unref(caps);
}

TEST_F(WesterosSinkMainTest, SWProcessCapsPixelAspectRatio) {
    GstCaps *caps = gst_caps_new_simple("video/x-h264",
                                         "pixel-aspect-ratio", GST_TYPE_FRACTION, 4, 3,
                                         NULL);
    wstsw_process_caps(&sink, caps);
    gst_caps_unref(caps);
}

TEST_F(WesterosSinkMainTest, SWProcessCapsNullContext) {
    GstCaps *caps = gst_caps_new_empty();
    wstsw_process_caps(&sink, caps);
    gst_caps_unref(caps);
}

TEST_F(WesterosSinkMainTest, SWSetCodecInitDataValid) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x28};
    wstsw_set_codec_init_data(&sink, sizeof(data), data);
}

TEST_F(WesterosSinkMainTest, SWSetCodecInitDataLarge) {
    uint8_t largeData[512];
    std::memset(largeData, 0xFF, sizeof(largeData));
    wstsw_set_codec_init_data(&sink, sizeof(largeData), largeData);
}

TEST_F(WesterosSinkMainTest, SWResetTimeBasic) {
    wstsw_reset_time(&sink);
}

TEST_F(WesterosSinkMainTest, SWResetTimeMultiple) {
    for (int i = 0; i < 10; i++) {
        wstsw_reset_time(&sink);
    }
}

// ============================================================================
// PROPERTY OPERATIONS
// ============================================================================

TEST_F(WesterosSinkMainTest, SetPropertyWindowSet) {
    gst_westeros_sink_set_property((GObject*)&sink, 1, &value_string, &pspec);
}

TEST_F(WesterosSinkMainTest, SetPropertyZOrder) {
    gst_westeros_sink_set_property((GObject*)&sink, 2, &value_float, &pspec);
}

TEST_F(WesterosSinkMainTest, SetPropertyOpacity) {
    gst_westeros_sink_set_property((GObject*)&sink, 3, &value_float, &pspec);
}

TEST_F(WesterosSinkMainTest, SetPropertyVisible) {
    gst_westeros_sink_set_property((GObject*)&sink, 4, &value_boolean, &pspec);
}

TEST_F(WesterosSinkMainTest, SetPropertyZoomMode) {
    gst_westeros_sink_set_property((GObject*)&sink, 5, &value_int, &pspec);
}

TEST_F(WesterosSinkMainTest, GetPropertyMultiple) {
    for (guint i = 1; i <= 10; i++) {
        gst_westeros_sink_get_property((GObject*)&sink, i, &value_int, &pspec);
    }
}

TEST_F(WesterosSinkMainTest, PropertySetGetCycle) {
    for (int cycle = 0; cycle < 3; cycle++) {
        gst_westeros_sink_set_property((GObject*)&sink, 1, &value_string, &pspec);
        gst_westeros_sink_get_property((GObject*)&sink, 1, &value_string, &pspec);
    }
}

// ============================================================================
// BACKEND STATE TRANSITIONS
// ============================================================================

TEST_F(WesterosSinkMainTest, BackendNullToReady) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_null_to_ready(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendReadyToPaused) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_ready_to_paused(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendPausedToPlaying) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_paused_to_playing(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendPlayingToPaused) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_playing_to_paused(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendPausedToReady) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_paused_to_ready(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendReadyToNull) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_ready_to_null(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendFullCycle) {
    gboolean pass = FALSE;
    gst_westeros_sink_backend_null_to_ready(&sink, &pass);
    gst_westeros_sink_backend_ready_to_paused(&sink, &pass);
    gst_westeros_sink_backend_paused_to_playing(&sink, &pass);
    gst_westeros_sink_backend_playing_to_paused(&sink, &pass);
    gst_westeros_sink_backend_paused_to_ready(&sink, &pass);
    gst_westeros_sink_backend_ready_to_null(&sink, &pass);
}

TEST_F(WesterosSinkMainTest, BackendMultipleCycles) {
    gboolean pass = FALSE;
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_backend_null_to_ready(&sink, &pass);
        gst_westeros_sink_backend_ready_to_paused(&sink, &pass);
        gst_westeros_sink_backend_paused_to_ready(&sink, &pass);
        gst_westeros_sink_backend_ready_to_null(&sink, &pass);
    }
}

// ============================================================================
// QUERY AND EVENT HANDLING
// ============================================================================

TEST_F(WesterosSinkMainTest, QueryNullQuery) {
    GstElement elem;
    std::memset(&elem, 0, sizeof(elem));
    gst_westeros_sink_query(&elem, nullptr);
}

TEST_F(WesterosSinkMainTest, QueryValidQuery) {
    GstElement elem;
    GstQuery query;
    std::memset(&elem, 0, sizeof(elem));
    std::memset(&query, 0, sizeof(query));
    gst_westeros_sink_query(&elem, &query);
}

TEST_F(WesterosSinkMainTest, SendEventNull) {
    GstElement elem;
    std::memset(&elem, 0, sizeof(elem));
    gst_westeros_sink_send_event(&elem, nullptr);
}

TEST_F(WesterosSinkMainTest, SendEventValid) {
    GstElement elem;
    GstEvent event;
    std::memset(&elem, 0, sizeof(elem));
    std::memset(&event, 0, sizeof(event));
    gst_westeros_sink_send_event(&elem, &event);
}

// ============================================================================
// RENDER AND PREROLL
// ============================================================================

TEST_F(WesterosSinkMainTest, RenderNullBuffer) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_render(&base, nullptr);
}

TEST_F(WesterosSinkMainTest, RenderValidBuffer) {
    GstBaseSink base;
    GstBuffer buf;
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    gst_westeros_sink_render(&base, &buf);
}

TEST_F(WesterosSinkMainTest, RenderMultipleBuffers) {
    GstBaseSink base;
    GstBuffer buf;
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_render(&base, &buf);
    }
}

TEST_F(WesterosSinkMainTest, PrerollNullBuffer) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_preroll(&base, nullptr);
}

TEST_F(WesterosSinkMainTest, PrerollValidBuffer) {
    GstBaseSink base;
    GstBuffer buf;
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    gst_westeros_sink_preroll(&base, &buf);
}

TEST_F(WesterosSinkMainTest, RenderPrerollInterleaved) {
    GstBaseSink base;
    GstBuffer buf;
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_render(&base, &buf);
        gst_westeros_sink_preroll(&base, &buf);
    }
}

// ============================================================================
// START/STOP/UNLOCK
// ============================================================================

TEST_F(WesterosSinkMainTest, StartBasic) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_start(&base);
}

TEST_F(WesterosSinkMainTest, StopBasic) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_stop(&base);
}

TEST_F(WesterosSinkMainTest, StartStopCycle) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_start(&base);
    gst_westeros_sink_stop(&base);
}

TEST_F(WesterosSinkMainTest, UnlockBasic) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_unlock(&base);
}

TEST_F(WesterosSinkMainTest, UnlockStopBasic) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    gst_westeros_sink_unlock_stop(&base);
}

TEST_F(WesterosSinkMainTest, UnlockCycle) {
    GstBaseSink base;
    std::memset(&base, 0, sizeof(base));
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_unlock(&base);
        gst_westeros_sink_unlock_stop(&base);
    }
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(WesterosSinkMainTest, PropertyStress) {
    for (int c = 0; c < 5; c++) {
        for (guint id = 0; id < 15; id++) {
            GValue *v = (id % 2) ? &value_int : &value_float;
            gst_westeros_sink_set_property((GObject*)&sink, id, v, &pspec);
            gst_westeros_sink_get_property((GObject*)&sink, id, v, &pspec);
        }
    }
}

TEST_F(WesterosSinkMainTest, BackendStateStress) {
    gboolean pass = FALSE;
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_backend_null_to_ready(&sink, &pass);
        gst_westeros_sink_backend_ready_to_paused(&sink, &pass);
        gst_westeros_sink_backend_paused_to_ready(&sink, &pass);
        gst_westeros_sink_backend_ready_to_null(&sink, &pass);
    }
}

TEST_F(WesterosSinkMainTest, RenderStress) {
    GstBaseSink base;
    GstBuffer buf;
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    for (int i = 0; i < 50; i++) {
        gst_westeros_sink_render(&base, &buf);
    }
}

TEST_F(WesterosSinkMainTest, CombinedOperations) {
    GstBaseSink base;
    GstBuffer buf;
    gboolean pass = FALSE;
    
    std::memset(&base, 0, sizeof(base));
    std::memset(&buf, 0, sizeof(buf));
    
    gst_westeros_sink_backend_null_to_ready(&sink, &pass);
    gst_westeros_sink_start(&base);
    gst_westeros_sink_render(&base, &buf);
    gst_westeros_sink_preroll(&base, &buf);
    gst_westeros_sink_stop(&base);
    gst_westeros_sink_backend_ready_to_null(&sink, &pass);
}