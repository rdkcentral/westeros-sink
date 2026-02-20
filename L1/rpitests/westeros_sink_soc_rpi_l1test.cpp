/*
 * L1 Test Suite for Westeros-Sink RPI SOC Implementation
 * Coverage: rpi/westeros-sink-soc.c
 * Target: 100% function and line coverage
 */

#include <gtest/gtest.h>
#include <glib.h>
#include <gst/gst.h>
#include <cstring>

extern "C" {
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
}

namespace {

class WesterosSinkSocRpiTest : public ::testing::Test {
protected:
    GstWesterosSink sink;
    gboolean passToDefault;
    GValue gval;
    GParamSpec gspec;
    GstCaps *caps;
    GstBuffer buffer;
    GstQuery query;

    void SetUp() override {
        GError *error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &error)) {
            if (error) g_error_free(error);
        }
        
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
        sink.outputWidth = 1280;
        sink.outputHeight = 720;
        sink.visible = TRUE;
        sink.windowX = 0;
        sink.windowY = 0;
        sink.windowWidth = 1280;
        sink.windowHeight = 720;
        sink.transX = 0;
        sink.transY = 0;
        sink.videoStarted = FALSE;
        
        // RPI-specific initialization
        sink.soc.sb = NULL;
        sink.display = NULL;
    }
    
    void TearDown() override {
        if (caps) gst_caps_unref(caps);
        g_mutex_clear(&sink.mutex);
    }
};

// ============================================================================
// SOC INITIALIZATION AND TERMINATION
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, SocInitValidSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocRpiTest, SocInitMultipleTimes) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_init(&sink);
}

TEST_F(WesterosSinkSocRpiTest, SocTermBasic) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, SocTermWithoutInit) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, SocInitTermCycle) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_init(&sink);
        gst_westeros_sink_soc_term(&sink);
    }
}

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, StateNullToReady) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StateReadyToPaused) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StatePausedToPlaying) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StatePlayingToPaused) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StatePlayingToPausedNoVideo) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StatePausedToReady) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StatePausedToReadyNoVideo) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StateReadyToNull) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRpiTest, StateFullLifecycle) {
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

TEST_F(WesterosSinkSocRpiTest, StateNullPassFlag) {
    gst_westeros_sink_soc_null_to_ready(&sink, nullptr);
    gst_westeros_sink_soc_ready_to_paused(&sink, nullptr);
    gst_westeros_sink_soc_paused_to_playing(&sink, nullptr);
}

TEST_F(WesterosSinkSocRpiTest, StateMultipleCycles) {
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

TEST_F(WesterosSinkSocRpiTest, AcceptCapsEmpty) {
    EXPECT_TRUE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocRpiTest, AcceptCapsWithFormat) {
    gst_caps_unref(caps);
    caps = gst_caps_new_simple("video/x-h264",
                                "width", G_TYPE_INT, 1280,
                                "height", G_TYPE_INT, 720,
                                NULL);
    EXPECT_TRUE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocRpiTest, AcceptCapsWithFramerate) {
    gst_caps_unref(caps);
    caps = gst_caps_new_simple("video/x-h264",
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                NULL);
    EXPECT_TRUE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocRpiTest, AcceptCapsNull) {
    EXPECT_TRUE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocRpiTest, StartVideoBasic) {
    EXPECT_TRUE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocRpiTest, StartVideoMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_start_video(&sink);
    }
}

// ============================================================================
// QUERY HANDLING
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, QueryNull) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocRpiTest, QueryValid) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocRpiTest, QueryMultiple) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_query(&sink, &query);
    }
}

// ============================================================================
// CLASS INITIALIZATION
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, ClassInitNull) {
    gst_westeros_sink_soc_class_init(nullptr);
}

TEST_F(WesterosSinkSocRpiTest, ClassInitValid) {
    GstWesterosSinkClass klass;
    std::memset(&klass, 0, sizeof(klass));
    gst_westeros_sink_soc_class_init(&klass);
}

TEST_F(WesterosSinkSocRpiTest, ClassInitMultiple) {
    GstWesterosSinkClass klass;
    std::memset(&klass, 0, sizeof(klass));
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_class_init(&klass);
    }
}

// ============================================================================
// PROPERTY OPERATIONS
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, SetPropertyNull) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocRpiTest, SetPropertyValidValue) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, 42);
    
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &val, &gspec);
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, SetPropertyMultiple) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int i = 1; i <= 20; i++) {
        g_value_set_int(&val, i);
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &val, nullptr);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, GetPropertyNull) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocRpiTest, GetPropertyValidValue) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &val, &gspec);
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, GetPropertyMultiple) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &val, nullptr);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, PropertySetGetCycle) {
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

TEST_F(WesterosSinkSocRpiTest, SetStartPTSZero) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocRpiTest, SetStartPTSPositive) {
    gst_westeros_sink_soc_set_startPTS(&sink, 90000);
}

TEST_F(WesterosSinkSocRpiTest, SetStartPTSLarge) {
    gst_westeros_sink_soc_set_startPTS(&sink, 9000000000LL);
}

TEST_F(WesterosSinkSocRpiTest, SetStartPTSNegative) {
    gst_westeros_sink_soc_set_startPTS(&sink, -1);
}

TEST_F(WesterosSinkSocRpiTest, SetStartPTSSequence) {
    for (gint64 pts = 0; pts <= 100000; pts += 10000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocRpiTest, SetStartPTSRandom) {
    gint64 ptsList[] = {0, 1000, 90000, 180000, 270000, 360000, 450000};
    for (size_t i = 0; i < sizeof(ptsList)/sizeof(ptsList[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, ptsList[i]);
    }
}

// ============================================================================
// RENDER AND FLUSH
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, RenderNull) {
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocRpiTest, RenderValid) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRpiTest, RenderMultipleBuffers) {
    for (int i = 0; i < 25; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocRpiTest, FlushBasic) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocRpiTest, FlushMultiple) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocRpiTest, RenderFlushSequence) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_flush(&sink);
    }
}

// ============================================================================
// EOS AND VIDEO PATH
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, EosEventBasic) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocRpiTest, EosEventMultiple) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

TEST_F(WesterosSinkSocRpiTest, SetVideoPathFalse) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocRpiTest, SetVideoPathTrue) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocRpiTest, SetVideoPathToggle) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
    }
}

// ============================================================================
// VIDEO POSITION UPDATE
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, UpdateVideoPositionDefault) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocRpiTest, UpdateVideoPositionCustom) {
    sink.windowX = 100;
    sink.windowY = 100;
    sink.windowWidth = 640;
    sink.windowHeight = 480;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocRpiTest, UpdateVideoPositionMultiple) {
    for (int i = 0; i < 10; i++) {
        sink.windowX = i * 10;
        sink.windowY = i * 10;
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

TEST_F(WesterosSinkSocRpiTest, UpdateVideoPositionBoundary) {
    int positions[][4] = {
        {0, 0, 1280, 720},
        {100, 100, 640, 480},
        {0, 0, 1920, 1080},
        {640, 0, 640, 720},
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
// REGISTRY HANDLERS
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, RegistryHandleGlobalBasic) {
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, nullptr, 0);
}

TEST_F(WesterosSinkSocRpiTest, RegistryHandleGlobalRemoveBasic) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 0);
}

TEST_F(WesterosSinkSocRpiTest, RegistryHandleGlobalMultiple) {
    for (uint32_t i = 0; i < 5; i++) {
        gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, i, nullptr, 0);
    }
}

TEST_F(WesterosSinkSocRpiTest, RegistryHandleGlobalRemoveMultiple) {
    for (uint32_t i = 0; i < 5; i++) {
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, i);
    }
}

// ============================================================================
// STRESS AND COMBINED TESTS
// ============================================================================

TEST_F(WesterosSinkSocRpiTest, StressFullOperationCycle) {
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

TEST_F(WesterosSinkSocRpiTest, StressRenderSequence) {
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

TEST_F(WesterosSinkSocRpiTest, StressVideoPositionUpdate) {
    for (int x = 0; x <= 1000; x += 100) {
        for (int y = 0; y <= 1000; y += 100) {
            sink.windowX = x;
            sink.windowY = y;
            sink.windowWidth = 640;
            sink.windowHeight = 480;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocRpiTest, CombinedAllOperations) {
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
    
    // Registry operations
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 1, nullptr, 0);
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 1);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, InterleavedStateAndRender) {
    gst_westeros_sink_soc_init(&sink);
    
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        
        // Render some frames
        for (int j = 0; j < 3; j++) {
            gst_westeros_sink_soc_set_startPTS(&sink, (i * 3 + j) * 3000);
            gst_westeros_sink_soc_render(&sink, &buffer);
        }
        
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, VideoPathSwitchingStress) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Rapidly switch video paths
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_update_video_position(&sink);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, CapabilitiesNegotiation) {
    gst_westeros_sink_soc_init(&sink);
    
    // Test different cap formats
    GstCaps *testCaps[] = {
        gst_caps_new_simple("video/x-h264", "width", G_TYPE_INT, 1280, "height", G_TYPE_INT, 720, NULL),
        gst_caps_new_simple("video/x-h264", "framerate", GST_TYPE_FRACTION, 30, 1, NULL),
        gst_caps_new_simple("video/mpeg", "width", G_TYPE_INT, 720, "height", G_TYPE_INT, 480, NULL),
        gst_caps_new_empty()
    };
    
    for (size_t i = 0; i < sizeof(testCaps)/sizeof(testCaps[0]); i++) {
        gst_westeros_sink_soc_accept_caps(&sink, testCaps[i]);
        gst_caps_unref(testCaps[i]);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, PropertyExtensiveTesting) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    // Test property IDs exhaustively
    for (int propId = 1; propId <= 30; propId++) {
        g_value_set_int(&val, propId * 100);
        gst_westeros_sink_soc_set_property((GObject*)&sink, propId, &val, &gspec);
        gst_westeros_sink_soc_get_property((GObject*)&sink, propId, &val, &gspec);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, TimingAndSynchronization) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Test various PTS timing scenarios
    gint64 ptsList[] = {
        0,                      // Zero PTS
        GST_MSECOND,           // 1ms
        GST_SECOND/30,         // 30fps frame duration
        GST_SECOND/24,         // 24fps frame duration
        GST_SECOND,            // 1 second
        GST_SECOND * 60,       // 1 minute
    };
    
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

TEST_F(WesterosSinkSocRpiTest, ExtendedInitTermCycles) {
    // Test 10 cycles to ensure robust handling
    for (int cycle = 0; cycle < 10; cycle++) {
        EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
        gst_westeros_sink_soc_term(&sink);
    }
}

TEST_F(WesterosSinkSocRpiTest, ComplexStateTransitions) {
    // More complex state machine traversal
    gst_westeros_sink_soc_init(&sink);
    
    // Transition 1
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
    
    // Render during playing
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    // Back to paused
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
    
    // Pause to ready to null
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, VideoPathToggleStress) {
    gst_westeros_sink_soc_init(&sink);
    
    for (int i = 0; i < 50; i++) {
        bool path = (i % 2) == 0;
        gst_westeros_sink_soc_set_video_path(&sink, path);
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_update_video_position(&sink);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, MultipleCapFormats) {
    // Test with various cap formats
    for (int i = 0; i < 20; i++) {
        GstCaps *testCaps = gst_caps_new_simple("video/x-h264",
                                                 "width", G_TYPE_INT, 1920 - i*100,
                                                 "height", G_TYPE_INT, 1080 - i*50,
                                                 NULL);
        EXPECT_TRUE(gst_westeros_sink_soc_accept_caps(&sink, testCaps));
        gst_caps_unref(testCaps);
    }
}

TEST_F(WesterosSinkSocRpiTest, PropertySetGetInterleaved) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int i = 0; i < 50; i++) {
        g_value_set_int(&val, i);
        gst_westeros_sink_soc_set_property((GObject*)&sink, i % 30, &val, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, (i+15) % 30, &val, nullptr);
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, RenderWithVariedPTS) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    gint64 ptsValues[] = {
        0, 1000, 3000, 5000, 10000, 30000, 90000,
        180000, 300000, 900000, 1000000, 5000000
    };
    
    for (size_t i = 0; i < sizeof(ptsValues)/sizeof(ptsValues[0]); i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, ptsValues[i]);
        for (int j = 0; j < 3; j++) {
            gst_westeros_sink_soc_render(&sink, &buffer);
        }
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, FlushDuringVariousStates) {
    gst_westeros_sink_soc_init(&sink);
    
    // Flush in init state
    gst_westeros_sink_soc_flush(&sink);
    
    // Flush after null_to_ready
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_flush(&sink);
    
    // Flush after ready_to_paused
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_flush(&sink);
    
    // Multiple flushes
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
    
    // Flush after playing_to_paused
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_flush(&sink);
    
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, EosEventVariedTiming) {
    gst_westeros_sink_soc_init(&sink);
    
    // EOS early
    gst_westeros_sink_soc_eos_event(&sink);
    
    // EOS in different states
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_eos_event(&sink);
    
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_eos_event(&sink);
    
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_eos_event(&sink);
    
    // Multiple EOS events
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, QueryDifferentStates) {
    gst_westeros_sink_soc_init(&sink);
    
    // Query in various states
    for (int state = 0; state < 5; state++) {
        for (int i = 0; i < 5; i++) {
            gst_westeros_sink_soc_query(&sink, &query);
            gst_westeros_sink_soc_query(&sink, nullptr);
        }
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, WindowPositionVariations) {
    int positions[] = {0, 100, 640, 1280, 1920, 3840};
    int sizes[] = {480, 720, 1080, 2160, 4320};
    
    for (size_t x = 0; x < sizeof(positions)/sizeof(positions[0]); x++) {
        for (size_t y = 0; y < sizeof(positions)/sizeof(positions[0]); y++) {
            for (size_t w = 0; w < sizeof(sizes)/sizeof(sizes[0]); w++) {
                sink.windowX = positions[x];
                sink.windowY = positions[y];
                sink.windowWidth = sizes[w];
                sink.windowHeight = sizes[y];
                gst_westeros_sink_soc_update_video_position(&sink);
            }
        }
    }
}

TEST_F(WesterosSinkSocRpiTest, StartVideoInDifferentStates) {
    // Start video before any state transition
    gst_westeros_sink_soc_start_video(&sink);
    
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_start_video(&sink);
    
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_start_video(&sink);
    
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_start_video(&sink);
    
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_start_video(&sink);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, RegistryOperationsVaried) {
    gst_westeros_sink_soc_init(&sink);
    
    // Multiple registry operations
    for (uint32_t id = 0; id < 100; id++) {
        gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, id, nullptr, 1);
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, id);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, ScalingFactorVariations) {
    gst_westeros_sink_soc_init(&sink);
    
    // Test various scaling factors
    int scales[][4] = {
        {1, 1, 1, 1},    // 1:1
        {2, 1, 1, 1},    // 2x horizontal
        {1, 2, 1, 1},    // 2x vertical
        {2, 2, 1, 1},    // 2x both
        {3, 2, 1, 1},    // 3:2 horizontal
        {1, 1, 2, 2},    // divided by 2
        {4, 4, 1, 1},    // 4x scale
    };
    
    for (size_t i = 0; i < sizeof(scales)/sizeof(scales[0]); i++) {
        sink.scaleXNum = scales[i][0];
        sink.scaleXDenom = scales[i][1];
        sink.scaleYNum = scales[i][2];
        sink.scaleYDenom = scales[i][3];
        gst_westeros_sink_soc_update_video_position(&sink);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, NullChecksAndEdgeCases) {
    // Call with nullptr where appropriate
    gst_westeros_sink_soc_accept_caps(&sink, nullptr);
    gst_westeros_sink_soc_query(&sink, nullptr);
    gst_westeros_sink_soc_render(&sink, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
    
    // Null class init
    gst_westeros_sink_soc_class_init(nullptr);
}

TEST_F(WesterosSinkSocRpiTest, TransitionsWithNullPassFlag) {
    gst_westeros_sink_soc_init(&sink);
    
    // Transitions with nullptr passToDefault flag
    gst_westeros_sink_soc_null_to_ready(&sink, nullptr);
    gst_westeros_sink_soc_ready_to_paused(&sink, nullptr);
    gst_westeros_sink_soc_paused_to_playing(&sink, nullptr);
    gst_westeros_sink_soc_playing_to_paused(&sink, nullptr);
    gst_westeros_sink_soc_paused_to_ready(&sink, nullptr);
    gst_westeros_sink_soc_ready_to_null(&sink, nullptr);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, RapidRenderFlushCycle) {
    gst_westeros_sink_soc_init(&sink);
    
    // Rapid render-flush cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        for (int i = 0; i < 50; i++) {
            gst_westeros_sink_soc_render(&sink, &buffer);
            if (i % 5 == 0) {
                gst_westeros_sink_soc_flush(&sink);
            }
        }
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, PropertyExtensiveRange) {
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    // Test many property IDs with various values
    for (int propId = 0; propId < 100; propId++) {
        for (int value = -10; value <= 10; value++) {
            g_value_set_int(&val, value);
            gst_westeros_sink_soc_set_property((GObject*)&sink, propId, &val, nullptr);
            gst_westeros_sink_soc_get_property((GObject*)&sink, propId, &val, nullptr);
        }
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, FullOperationSequenceNoErrors) {
    // Comprehensive sequence covering many code paths
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    
    for (int capsIdx = 0; capsIdx < 3; capsIdx++) {
        GstCaps *testCaps = gst_caps_new_simple("video/x-h264",
                                                 "width", G_TYPE_INT, 1280,
                                                 "height", G_TYPE_INT, 720,
                                                 NULL);
        gst_westeros_sink_soc_accept_caps(&sink, testCaps);
        gst_caps_unref(testCaps);
    }
    
    gst_westeros_sink_soc_start_video(&sink);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
        
        gst_westeros_sink_soc_set_startPTS(&sink, i * 90000);
        for (int j = 0; j < 10; j++) {
            gst_westeros_sink_soc_render(&sink, &buffer);
        }
        
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
        gst_westeros_sink_soc_update_video_position(&sink);
        
        sink.videoStarted = TRUE;
        gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_flush(&sink);
        gst_westeros_sink_soc_eos_event(&sink);
        gst_westeros_sink_soc_query(&sink, &query);
        
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    }
    
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, VideoStartedStateVariations) {
    gst_westeros_sink_soc_init(&sink);
    
    // Test with videoStarted = FALSE
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    // Test with videoStarted = TRUE
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, MixedPropertyTypes) {
    // Test set/get with different property IDs in sequence
    GValue val;
    std::memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    
    for (int cycle = 0; cycle < 5; cycle++) {
        for (guint propId = 1; propId <= 30; propId++) {
            g_value_set_int(&val, propId * 10);
            gst_westeros_sink_soc_set_property((GObject*)&sink, propId, &val, &gspec);
            
            g_value_set_int(&val, 0);
            gst_westeros_sink_soc_get_property((GObject*)&sink, propId, &val, &gspec);
        }
    }
    
    g_value_unset(&val);
}

TEST_F(WesterosSinkSocRpiTest, BufferRenderVariations) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    // Render with different buffer configurations
    GstBuffer buffers[5];
    for (int i = 0; i < 5; i++) {
        std::memset(&buffers[i], 0, sizeof(GstBuffer));
        gst_westeros_sink_soc_render(&sink, &buffers[i]);
    }
    
    // Render null buffer
    gst_westeros_sink_soc_render(&sink, nullptr);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRpiTest, AcceptCapsEdgeCases) {
    gst_westeros_sink_soc_init(&sink);
    
    // Accept caps with various configurations
    for (int width = 320; width <= 3840; width *= 2) {
        for (int height = 240; height <= 2160; height *= 2) {
            GstCaps *testCaps = gst_caps_new_simple("video/x-h264",
                                                     "width", G_TYPE_INT, width,
                                                     "height", G_TYPE_INT, height,
                                                     NULL);
            gst_westeros_sink_soc_accept_caps(&sink, testCaps);
            gst_caps_unref(testCaps);
        }
    }
    
    gst_westeros_sink_soc_term(&sink);
}

} // namespace

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    int test_result = RUN_ALL_TESTS();
    
    return test_result;
}
