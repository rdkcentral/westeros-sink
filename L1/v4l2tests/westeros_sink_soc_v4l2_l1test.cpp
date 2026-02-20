/*
 * L1 Test Suite for Westeros-Sink V4L2 SOC Implementation
 */

#include <gtest/gtest.h>
#include <glib.h>
#include <gst/gst.h>
#include <cstring>
#include <linux/videodev2.h>

extern "C" {
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
}

namespace {

class WesterosSinkSocV4l2Test : public ::testing::Test {
protected:
    GstWesterosSink sink;
    gboolean passToDefault;
    GValue gval;
    GParamSpec gspec;
    GstCaps *caps;
    GstBuffer buffer;
    GstQuery query;

    void SetUp() override {
        std::memset(&sink, 0, sizeof(sink));
        std::memset(&buffer, 0, sizeof(buffer));
        std::memset(&query, 0, sizeof(query));
        std::memset(&gval, 0, sizeof(gval));
        std::memset(&gspec, 0, sizeof(gspec));
        
        passToDefault = TRUE;
        caps = gst_caps_new_empty();
        
        // Initialize mutex
        g_mutex_init(&sink.mutex);
        
        // Set default values
        sink.scaleXDenom = 1;
        sink.scaleYDenom = 1;
        sink.scaleXNum = 1;
        sink.scaleYNum = 1;
        sink.outputWidth = 1920;
        sink.outputHeight = 1080;
        sink.visible = TRUE;
        sink.windowX = 0;
        sink.windowY = 0;
        sink.windowWidth = 1920;
        sink.windowHeight = 1080;
        sink.transX = 0;
        sink.transY = 0;
        sink.videoStarted = FALSE;
        
        // SOC-specific initialization
        sink.soc.v4l2Fd = -1;
        sink.soc.frameWidth = 1920;
        sink.soc.frameHeight = 1080;
        sink.soc.frameRate = 30.0;
        sink.soc.pixelAspectRatio = 1.0;
    }
    
    void TearDown() override {
        if (caps) gst_caps_unref(caps);
        g_mutex_clear(&sink.mutex);
    }
};

// ============================================================================
// SOC INITIALIZATION AND TERMINATION
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, SocInitValidSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocV4l2Test, SocInitMultipleTimes) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_init(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, SocTermBasic) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, SocTermWithoutInit) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, SocInitTermCycle) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_term(&sink);
    }
}

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, StateNullToReady) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StateReadyToPaused) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StatePausedToPlaying) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StatePlayingToPaused) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StatePlayingToPausedNoVideo) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StatePausedToReady) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StatePausedToReadyNoVideo) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StateReadyToNull) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocV4l2Test, StateFullLifecycle) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
    
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, StateNullPassFlag) {
    gst_westeros_sink_soc_null_to_ready(&sink, nullptr);
    gst_westeros_sink_soc_ready_to_paused(&sink, nullptr);
    gst_westeros_sink_soc_paused_to_playing(&sink, nullptr);
}

TEST_F(WesterosSinkSocV4l2Test, StateMultipleCycles) {
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
        gst_westeros_sink_soc_term(&sink);
    }
}

// ============================================================================
// CAPS AND VIDEO START
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, AcceptCapsEmpty) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocV4l2Test, AcceptCapsWithFormat) {
    gst_caps_unref(caps);
    caps = gst_caps_new_simple("video/x-h264",
                                "width", G_TYPE_INT, 1920,
                                "height", G_TYPE_INT, 1080,
                                NULL);
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocV4l2Test, AcceptCapsWithFramerate) {
    gst_caps_unref(caps);
    caps = gst_caps_new_simple("video/x-h264",
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                NULL);
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocV4l2Test, AcceptCapsNull) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocV4l2Test, StartVideoBasic) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocV4l2Test, StartVideoMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_start_video(&sink);
    }
}

// ============================================================================
// QUERY HANDLING
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, QueryNull) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocV4l2Test, QueryValid) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocV4l2Test, QueryMultiple) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_query(&sink, &query);
    }
}

// ============================================================================
// CLASS INITIALIZATION
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, ClassInitNull) {
    gst_westeros_sink_soc_class_init(nullptr);
}

TEST_F(WesterosSinkSocV4l2Test, ClassInitValid) {
    GstWesterosSinkClass klass;
    std::memset(&klass, 0, sizeof(klass));
    gst_westeros_sink_soc_class_init(&klass);
}

TEST_F(WesterosSinkSocV4l2Test, ClassInitMultiple) {
    GstWesterosSinkClass klass;
    std::memset(&klass, 0, sizeof(klass));
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_class_init(&klass);
    }
}

// ============================================================================
// PROPERTY OPERATIONS
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, SetPropertyNull) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocV4l2Test, SetPropertyValidValue) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, 42);
    
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &val, &gspec);
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocV4l2Test, SetPropertyMultiple) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int i = 1; i <= 20; i++) {
        g_value_set_int(&val, i);
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &val, nullptr);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocV4l2Test, GetPropertyNull) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocV4l2Test, GetPropertyValidValue) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &val, &gspec);
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocV4l2Test, GetPropertyMultiple) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &val, nullptr);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocV4l2Test, PropertySetGetCycle) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int id = 1; id <= 10; id++) {
            g_value_set_int(&val, id * 10);
            gst_westeros_sink_soc_set_property((GObject*)&sink, id, &val, nullptr);
            gst_westeros_sink_soc_get_property((GObject*)&sink, id, &val, nullptr);
        }
    }
    
    g_value_unset(&val);
}

// ============================================================================
// PTS AND TIMING
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSZero) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSPositive) {
    gst_westeros_sink_soc_set_startPTS(&sink, 90000);
}

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSLarge) {
    gst_westeros_sink_soc_set_startPTS(&sink, 9000000000LL);
}

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSNegative) {
    gst_westeros_sink_soc_set_startPTS(&sink, -1);
}

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSSequence) {
    for (gint64 pts = 0; pts <= 100000; pts += 10000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocV4l2Test, SetStartPTSRandom) {
    gint64 ptsList[] = {0, 1000, 90000, 180000, 270000, 360000, 450000};
    for (size_t i = 0; i < sizeof(ptsList)/sizeof(ptsList[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, ptsList[i]);
    }
}

// ============================================================================
// RENDER AND FLUSH
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, RenderNull) {
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocV4l2Test, RenderValid) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocV4l2Test, RenderMultipleBuffers) {
    for (int i = 0; i < 25; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocV4l2Test, FlushBasic) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, FlushMultiple) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocV4l2Test, RenderFlushSequence) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_flush(&sink);
    }
}

// ============================================================================
// EOS AND VIDEO PATH
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, EosEventBasic) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, EosEventMultiple) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

TEST_F(WesterosSinkSocV4l2Test, SetVideoPathFalse) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocV4l2Test, SetVideoPathTrue) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocV4l2Test, SetVideoPathToggle) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
    }
}

// ============================================================================
// VIDEO POSITION UPDATE
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, UpdateVideoPositionDefault) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UpdateVideoPositionCustom) {
    sink.windowX = 100;
    sink.windowY = 100;
    sink.windowWidth = 800;
    sink.windowHeight = 600;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UpdateVideoPositionMultiple) {
    for (int i = 0; i < 10; i++) {
        sink.windowX = i * 10;
        sink.windowY = i * 10;
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

TEST_F(WesterosSinkSocV4l2Test, UpdateVideoPositionBoundary) {
    int positions[][4] = {
        {0, 0, 1920, 1080},
        {100, 100, 800, 600},
        {0, 0, 3840, 2160},
        {1920, 0, 1920, 1080},
    };
    
    for (size_t i = 0; i < sizeof(positions)/sizeof(positions[0]); i++) {
        sink.windowX = positions[i][0];
        sink.windowY = positions[i][1];
        sink.windowWidth = positions[i][2];
        sink.windowHeight = positions[i][3];
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

// ============================================================================
// STRESS AND COMBINED TESTS
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, StressFullOperationCycle) {
    for (int cycle = 0; cycle < 3; cycle++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
        
        gst_westeros_sink_soc_set_startPTS(&sink, cycle * 90000);
        
        for (int i = 0; i < 5; i++) {
            gst_westeros_sink_soc_render(&sink, &buffer);
        }
        
        gst_westeros_sink_soc_flush(&sink);
        gst_westeros_sink_soc_eos_event(&sink);
        
        sink.videoStarted = TRUE;
        gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
        gst_westeros_sink_soc_term(&sink);
    }
}

TEST_F(WesterosSinkSocV4l2Test, StressRenderSequence) {
    gst_westeros_sink_soc_init(&sink);
    
    for (int i = 0; i < 100; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
        
        if (i % 10 == 0) {
            gst_westeros_sink_soc_flush(&sink);
        }
        
        if (i % 20 == 0) {
            gst_westeros_sink_soc_set_startPTS(&sink, i * 1000);
        }
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, StressVideoPositionUpdate) {
    for (int x = 0; x <= 1000; x += 100) {
        for (int y = 0; y <= 1000; y += 100) {
            sink.windowX = x;
            sink.windowY = y;
            sink.windowWidth = 800;
            sink.windowHeight = 600;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocV4l2Test, CombinedAllOperations) {
    // Full sequence exercising all SOC functions
    GstWesterosSinkClass klass;
    std::memset(&klass, 0, sizeof(klass));
    gst_westeros_sink_soc_class_init(&klass);
    
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_accept_caps(&sink, caps);
    gst_westeros_sink_soc_start_video(&sink);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    gst_westeros_sink_soc_set_startPTS(&sink, 90000);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_update_video_position(&sink);
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_query(&sink, &query);
    
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &val, nullptr);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &val, nullptr);
    g_value_unset(&val);
    
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_eos_event(&sink);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

// ============================================================================
// UTILITY FUNCTION COVERAGE
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, UtilityNanoTimeToPTSZero) {
    // Test nanoTimeToPTS indirectly through set_startPTS which uses it
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityNanoTimeToPTSOneSecond) {
    gint64 nanoTime = GST_SECOND;
    gst_westeros_sink_soc_set_startPTS(&sink, nanoTime);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityNanoTimeToPTSVariadic) {
    // Test with various time values
    gint64 timings[] = {0, GST_MSECOND, GST_MSECOND * 100, GST_SECOND, GST_SECOND * 10};
    for (size_t i = 0; i < sizeof(timings)/sizeof(timings[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, timings[i]);
    }
}

TEST_F(WesterosSinkSocV4l2Test, UtilityFrameDebugLogging) {
    // Initialize sink to enable logging paths
    gst_westeros_sink_soc_init(&sink);
    
    // Execute operations that may trigger logging
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_set_startPTS(&sink, 90000);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityMultipleInitCycles) {
    // Test initialization and termination multiple times
    // to exercise internal state tracking and logging
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_set_startPTS(&sink, i * 90000);
        gst_westeros_sink_soc_term(&sink);
    }
}

TEST_F(WesterosSinkSocV4l2Test, UtilityBufferFullnessTracking) {
    // Initialize to set up SOC state
    gst_westeros_sink_soc_init(&sink);
    
    // Set buffer counts to exercise fullness tracking
    sink.soc.inQueuedCount = 1;
    sink.soc.numBuffersIn = 2;
    sink.soc.outQueuedCount = 3;
    sink.soc.numBuffersOut = 6;
    
    // Render operations that may check buffer fullness
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityStateTransitionsWithLogging) {
    gst_westeros_sink_soc_init(&sink);
    
    // Execute state transitions that exercise internal logging
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Multiple render calls to exercise timing and logging
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityPropertyAccessPatterns) {
    gst_westeros_sink_soc_init(&sink);
    
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, 42);
    
    // Exercise property access to trigger internal state logging
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &val, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &val, nullptr);
    }
    
    g_value_unset(&val);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityCapabilitiesNegotiation) {
    gst_westeros_sink_soc_init(&sink);
    
    // Test capabilities handling which exercises internal format checking
    GstCaps *testCaps = gst_caps_new_simple("video/x-h264",
                                            "width", G_TYPE_INT, 1920,
                                            "height", G_TYPE_INT, 1080,
                                            "framerate", GST_TYPE_FRACTION, 30, 1,
                                            NULL);
    
    gst_westeros_sink_soc_accept_caps(&sink, testCaps);
    
    gst_caps_unref(testCaps);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityVideoPathTransitions) {
    gst_westeros_sink_soc_init(&sink);
    
    // Exercise video path transitions which trigger internal state logging
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, true);
        gst_westeros_sink_soc_update_video_position(&sink);
        gst_westeros_sink_soc_set_video_path(&sink, false);
        gst_westeros_sink_soc_update_video_position(&sink);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityEventHandling) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Render frames before EOS to exercise timing calculations
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    // EOS event triggers internal time calculations and logging
    gst_westeros_sink_soc_eos_event(&sink);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityRenderWithVariousTimings) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Set various PTS values and render - exercises timing calculations
    gint64 ptsList[] = {0, 90000, 180000, 270000, 360000};
    for (size_t i = 0; i < sizeof(ptsList)/sizeof(ptsList[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, ptsList[i]);
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, UtilityExtendedStressTest) {
    // Long-running stress test to exercise all utility code paths
    const int CYCLES = 10;
    const int RENDERS_PER_CYCLE = 20;
    
    for (int cycle = 0; cycle < CYCLES; cycle++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
        
        // Vary PTS and render
        for (int i = 0; i < RENDERS_PER_CYCLE; i++) {
            gst_westeros_sink_soc_set_startPTS(&sink, cycle * RENDERS_PER_CYCLE * 90000 + i * 3000);
            gst_westeros_sink_soc_render(&sink, &buffer);
            
            if (i % 5 == 0) {
                gst_westeros_sink_soc_update_video_position(&sink);
            }
        }
        
        gst_westeros_sink_soc_flush(&sink);
        gst_westeros_sink_soc_eos_event(&sink);
        
        sink.videoStarted = TRUE;
        gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
        
        gst_westeros_sink_soc_term(&sink);
    }
}

// ============================================================================
// DIRECT TIMING UTILITY FUNCTION COVERAGE
// ============================================================================

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityNanoTimeToPTSExecution) {
    // Exercise nanoTimeToPTS indirectly through heavy timing operations
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Multiple PTS values force different code paths in nanoTimeToPTS
    gint64 nanoTimes[] = {
        0,
        1000000,           // 1 millisecond
        1000000000,        // 1 second
        45000000000LL,     // 45 seconds
        1000000000000LL,   // 1000 seconds
        0xFFFFFFFFFFFFLL   // Large value
    };
    
    for (size_t i = 0; i < sizeof(nanoTimes)/sizeof(nanoTimes[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, nanoTimes[i]);
        // Render to utilize PTS conversions
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityGetCurrentTimeMillis) {
    // getCurrentTimeMillis is called from frameLog during rendering
    // when timing information is needed
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Render multiple frames with different timings
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, i * 90000);  // Different PTS each frame
        gst_westeros_sink_soc_render(&sink, &buffer);
        
        // Occasional flushes trigger time-based operations
        if (i % 5 == 0) {
            gst_westeros_sink_soc_flush(&sink);
        }
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityGetGstClockTime) {
    // getGstClockTime is called when GStreamer clock operations occur
    // Set up sink with timing operations
    gst_westeros_sink_soc_init(&sink);
    
    // Initialize timing-related fields that may trigger clock access
    sink.scaleXDenom = 1;
    sink.scaleYDenom = 1;
    sink.scaleXNum = 1;
    sink.scaleYNum = 1;
    
    // State transitions may access clock information
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Render with timing information
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, GST_SECOND * i);
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityFrameLogging) {
    // frameLog uses getCurrentTimeMillis for timestamp generation
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Intensive frame rendering to trigger logging paths
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 30; i++) {
            gst_westeros_sink_soc_set_startPTS(&sink, cycle * 1000000 + i * 3000);
            gst_westeros_sink_soc_render(&sink, &buffer);
            
            if (i % 10 == 0) {
                gst_westeros_sink_soc_update_video_position(&sink);
            }
        }
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityExtendedRenderSequence) {
    // Extended render sequence to exercise all timing paths
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Render with varying frame rates and timings
    const int RENDER_COUNT = 50;
    for (int i = 0; i < RENDER_COUNT; i++) {
        // Simulate various PTS scenarios that exercise timing code
        gint64 pts = (i < 10) ? GST_MSECOND * i : 
                     (i < 20) ? GST_SECOND/30 * i :  // 30fps cadence
                     (i < 30) ? GST_SECOND/24 * i :  // 24fps cadence
                     GST_SECOND/60 * i;              // 60fps cadence
        
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    gst_westeros_sink_soc_flush(&sink);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityHighFrequencyOperations) {
    // High-frequency operations to maximize timing code execution
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Rapid-fire PTS updates and renders (higher frequency than real-time)
    for (int i = 0; i < 100; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, i * 1000);  // 1us increments
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocV4l2Test, TimingUtilityBoundaryValues) {
    // Test boundary values for PTS/timing calculations
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Boundary test values
    gint64 boundaryTimes[] = {
        0,                           // Zero
        1,                           // Minimum non-zero
        GST_MSECOND - 1,             // Just before millisecond
        GST_MSECOND,                 // Exact millisecond
        GST_MSECOND + 1,             // Just after millisecond
        (GST_SECOND/1000) - 1,       // Near millisecond boundary
        (GST_SECOND/1000),           // Exact millisecond conversion
        GST_SECOND - 1,              // Just before second
        GST_SECOND,                  // Exact second
        GST_SECOND + 1,              // Just after second
        0x7FFFFFFFFFFFFFFFLL - 1,    // Near max int64
    };
    
    for (size_t i = 0; i < sizeof(boundaryTimes)/sizeof(boundaryTimes[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, boundaryTimes[i]);
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

} // namespace

