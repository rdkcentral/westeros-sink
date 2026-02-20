#include <glib.h>
#include <gtest/gtest.h>
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
#include <cstring>

namespace {

// Mock GObject class for testing
static GObjectClass mock_gobject_class = {
    .g_type_class = {
        .g_type = G_TYPE_OBJECT
    }
};

static GObject mock_gobject_instance = {
    .g_type_instance = {
        .g_class = (GTypeClass*)&mock_gobject_class
    },
    .qdata = nullptr
};

class WesterosSinkSocRawTest : public ::testing::Test {
protected:
    GstWesterosSink sink;
    gboolean passToDefault;
    GObject gobj;
    GValue gval;
    GParamSpec gspec;
    GstCaps *caps;
    GstBuffer buffer;
    GstQuery query;

    void SetUp() override {
        std::memset(&sink, 0, sizeof(sink));
        passToDefault = TRUE;
        caps = gst_caps_new_empty();
        g_mutex_init(&sink.mutex);
        #ifdef GLIB_VERSION_2_32
        g_mutex_init(&sink.soc.mutex);
        #endif
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
        // Initialize buffer for render tests
        std::memset(&buffer, 0, sizeof(buffer));
    }
    
    void TearDown() override {
        if (caps) gst_caps_unref(caps);
        g_mutex_clear(&sink.mutex);
        #ifdef GLIB_VERSION_2_32
        g_mutex_clear(&sink.soc.mutex);
        #endif
    }
};

TEST_F(WesterosSinkSocRawTest, SocInitReturnsTrueForValidSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocRawTest, SocNullToReadyReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocReadyToPausedReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocPausedToPlayingReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocPlayingToPausedReturnsTrue) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocPausedToReadyReturnsTrue) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocReadyToNullReturnsTrue) {
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocAcceptCapsReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocRawTest, SocStartVideoReturnsTrue) {
    // RAW variant can successfully start video output
    EXPECT_TRUE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocRawTest, SocQueryReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocRawTest, SocTermDoesNotCrash) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocSetPropertyWithProvidedPspec) {
    // Test set_property with valid input
    GParamSpec pspec = {};
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gvalue, &pspec);
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocSetPropertyWithNullValue) {
    // Test with NULL value
    gst_westeros_sink_soc_set_property((GObject*)&sink, 100, nullptr, nullptr);
}

TEST_F(WesterosSinkSocRawTest, SocSetPropertyMultipleIds) {
    // Test with multiple property IDs
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_BOOLEAN);
    for (int id = 1; id <= 10; id++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, id, &gvalue, nullptr);
    }
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyWithProvidedPspec) {
    // Test get_property with valid input
    GParamSpec pspec = {};
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &gvalue, &pspec);
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyWithNullValue) {
    // Test with NULL value
    gst_westeros_sink_soc_get_property((GObject*)&sink, 100, nullptr, nullptr);
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyMultipleIds) {
    // Test with multiple property IDs
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_BOOLEAN);
    for (int id = 1; id <= 10; id++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, id, &gvalue, nullptr);
    }
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocSetStartPTSDoesNotCrash) {
    gst_westeros_sink_soc_set_startPTS(&sink, 12345);
}

TEST_F(WesterosSinkSocRawTest, SocRenderDoesNotCrash) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRawTest, SocFlushDoesNotCrash) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocEosEventDoesNotCrash) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocSetVideoPathDoesNotCrash) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionDoesNotCrash) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocFullLifecycle) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

// ============ Extended Tests for RAW ============
TEST_F(WesterosSinkSocRawTest, SocSetPropertyExtended) {
    for (int i = 1; i <= 15; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyExtended) {
    for (int i = 1; i <= 15; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocRawTest, SocSetStartPTSExtended) {
    for (gint64 pts = 0; pts <= 30000; pts += 5000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocRawTest, SocRenderExtended) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocRawTest, SocFlushExtended) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionExtended) {
    for (int x = 0; x <= 100; x += 50) {
        for (int y = 0; y <= 100; y += 50) {
            sink.windowX = x;
            sink.windowY = y;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocRawTest, SocStressTestState) {
    gst_westeros_sink_soc_init(&sink);
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
}

// ============ Comprehensive Property Tests ============
TEST_F(WesterosSinkSocRawTest, SocSetPropertyZeroPropertyId) {
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 0, &gvalue, nullptr);
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocSetPropertyHighPropertyId) {
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 999, &gvalue, nullptr);
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyZeroPropertyId) {
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 0, &gvalue, nullptr);
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocGetPropertyHighPropertyId) {
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 999, &gvalue, nullptr);
    g_value_unset(&gvalue);
}

// ============ Registry Event Tests ============
TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalMultipleInterfaces) {
    // Registry with NULL pointer (valid scenario per implementation)
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 1, "wl_sb", 1);
}

TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalWithUnknownInterface) {
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 5, "wl_unknown", 1);
}

TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalRemoveNull) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 0);
}

TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalRemoveMultiple) {
    for (uint32_t name = 0; name < 10; name++) {
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, name);
    }
}

// ============ State Transition with Video Started ============
TEST_F(WesterosSinkSocRawTest, SocPlayingToPausedWithVideoNotStarted) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocPausedToReadyWithVideoNotStarted) {
    sink.videoStarted = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocRawTest, SocReadyToNullWithPassToDefaultFalse) {
    passToDefault = FALSE;
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

// ============ Render Function Variations ============
TEST_F(WesterosSinkSocRawTest, SocRenderNullBuffer) {
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocRawTest, SocRenderWithVideoNotStarted) {
    sink.videoStarted = FALSE;
    sink.soc.haveHardware = FALSE;
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRawTest, SocRenderWithFlushStarted) {
    sink.flushStarted = TRUE;
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRawTest, SocRenderMultipleBuffers) {
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

// ============ Flush Function Variations ============
TEST_F(WesterosSinkSocRawTest, SocFlushWithVideoStarted) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocFlushWithVideoNotStarted) {
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocFlushMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

// ============ Start Video Function Variations ============
TEST_F(WesterosSinkSocRawTest, SocStartVideoReturnsTrueConsistently) {
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(gst_westeros_sink_soc_start_video(&sink));
    }
}

// ============ Query Function Variations ============
TEST_F(WesterosSinkSocRawTest, SocQueryWithNullQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocRawTest, SocQueryMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    }
}

// ============ EOS Event Variations ============
TEST_F(WesterosSinkSocRawTest, SocEosEventWithVideoStarted) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocEosEventWithVideoNotStarted) {
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocEosEventMultipleTimes) {
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

// ============ Video Path Control ============
TEST_F(WesterosSinkSocRawTest, SocSetVideoPathToggleMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        bool useGfx = (i % 2 == 0);
        gst_westeros_sink_soc_set_video_path(&sink, useGfx);
    }
}

TEST_F(WesterosSinkSocRawTest, SocSetVideoPathWithNegativeCoordinates) {
    sink.windowX = -100;
    sink.windowY = -100;
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocRawTest, SocSetVideoPathWithLargeCoordinates) {
    sink.windowX = 10000;
    sink.windowY = 10000;
    sink.windowWidth = 5000;
    sink.windowHeight = 5000;
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

// ============ Terminal Operations ============
TEST_F(WesterosSinkSocRawTest, SocTermAfterInit) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocTermWithoutInit) {
    // Term without prior init should handle gracefully
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocTermMultipleTimes) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_term(&sink);
    gst_westeros_sink_soc_term(&sink);
}

// ============ Start PTS Variations ============
TEST_F(WesterosSinkSocRawTest, SocSetStartPTSNegativeValue) {
    gst_westeros_sink_soc_set_startPTS(&sink, -1000LL);
}

TEST_F(WesterosSinkSocRawTest, SocSetStartPTSLargeValue) {
    gst_westeros_sink_soc_set_startPTS(&sink, 9223372036854775807LL);  // INT64_MAX
}

TEST_F(WesterosSinkSocRawTest, SocSetStartPTSSequentialValues) {
    for (gint64 pts = 0; pts < 100000; pts += 10000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

// ============ Accept Caps Variations ============
TEST_F(WesterosSinkSocRawTest, SocAcceptCapsWithValidCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocRawTest, SocAcceptCapsConsistency) {
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
    }
}

// ============ Update Video Position Variations ============
TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionWithScaling) {
    sink.scaleXNum = 2;
    sink.scaleYNum = 2;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionWithTransform) {
    sink.transX = 50;
    sink.transY = 50;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionWithVisibilityOff) {
    sink.visible = FALSE;
    gst_westeros_sink_soc_update_video_position(&sink);
}

// ============ Comprehensive Lifecycle Tests ============
TEST_F(WesterosSinkSocRawTest, SocPropertyOperationsInterleaved) {
    gst_westeros_sink_soc_init(&sink);
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gvalue, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gvalue, nullptr);
    }
    g_value_unset(&gvalue);
}

// ============ Class Initialization Tests ============
TEST_F(WesterosSinkSocRawTest, SocClassInitWithValidClass) {
    GstWesterosSinkClass raw_class = {};
    gst_westeros_sink_soc_class_init(&raw_class);
}

TEST_F(WesterosSinkSocRawTest, SocClassInitMultipleTimes) {
    GstWesterosSinkClass raw_class = {};
    gst_westeros_sink_soc_class_init(&raw_class);
    gst_westeros_sink_soc_class_init(&raw_class);
}

// ============ Accept Caps Deep Testing ============
TEST_F(WesterosSinkSocRawTest, SocAcceptCapsNullCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocRawTest, SocAcceptCapsEmptyCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

// ============ Init/Term Comprehensive Tests ============
TEST_F(WesterosSinkSocRawTest, SocInitMultipleTimesSequentially) {
    for (int i = 0; i < 3; i++) {
        std::memset(&sink, 0, sizeof(sink));
        g_mutex_init(&sink.mutex);
        EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
        gst_westeros_sink_soc_term(&sink);
    }
}

// ============ State Transition Edge Cases ============
TEST_F(WesterosSinkSocRawTest, SocStateTransitionNullToReadyMultipleTimes) {
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
    }
}

TEST_F(WesterosSinkSocRawTest, SocStateTransitionReadyToPausedMultipleTimes) {
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
    }
}

TEST_F(WesterosSinkSocRawTest, SocStateTransitionPausedToPlayingMultipleTimes) {
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
    }
}

TEST_F(WesterosSinkSocRawTest, SocStateTransitionReadyToNullMultipleTimes) {
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
    }
}

// ============ Render with Display Configuration ============
TEST_F(WesterosSinkSocRawTest, SocRenderWithDisplaySet) {
    sink.display = (wl_display*)0x1000;
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRawTest, SocRenderWithDisplayNotSet) {
    sink.display = nullptr;
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocRawTest, SocRenderWithFlushStartedMultipleTimes) {
    sink.flushStarted = TRUE;
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

// ============ Registry Comprehensive Tests ============
TEST_F(WesterosSinkSocRawTest, SocRegistryMultipleInterfaceNamesWithDifferentLengths) {
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 1, "a", 1);
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 2, "abc", 1);
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 3, "abcdefgh", 1);
    gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 4, "wl_sb_capture", 1);
}

TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalWithHighId) {
    for (uint32_t id = 1000; id < 1010; id++) {
        gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, id, "wl_sb", 1);
    }
}

TEST_F(WesterosSinkSocRawTest, SocRegistryHandleGlobalRemoveSequential) {
    for (uint32_t name = 0; name < 100; name++) {
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, name);
    }
}

// ============ Property Comprehensive Testing ============
TEST_F(WesterosSinkSocRawTest, SocPropertyAllIds) {
    GValue gvalue = {};
    g_value_init(&gvalue, G_TYPE_INT);
    for (int id = 0; id < 100; id += 5) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, id, &gvalue, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, id, &gvalue, nullptr);
    }
    g_value_unset(&gvalue);
}

TEST_F(WesterosSinkSocRawTest, SocPropertyWithDifferentValueTypes) {
    GValue gvalue_int = {};
    g_value_init(&gvalue_int, G_TYPE_INT);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gvalue_int, nullptr);
    g_value_unset(&gvalue_int);
    
    GValue gvalue_bool = {};
    g_value_init(&gvalue_bool, G_TYPE_BOOLEAN);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 2, &gvalue_bool, nullptr);
    g_value_unset(&gvalue_bool);
}

// ============ Video Position Update Comprehensive ============
TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionAllCombinations) {
    for (int scale = 1; scale <= 3; scale++) {
        for (int trans = 0; trans < 100; trans += 50) {
            sink.scaleXNum = scale;
            sink.scaleYNum = scale;
            sink.transX = trans;
            sink.transY = trans;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionWithDifferentDenominators) {
    for (int denom = 1; denom <= 5; denom++) {
        sink.scaleXDenom = denom;
        sink.scaleYDenom = denom;
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

TEST_F(WesterosSinkSocRawTest, SocUpdateVideoPositionWithWindowSizeOverride) {
    sink.windowSizeOverride = TRUE;
    for (int size = 100; size <= 2000; size += 300) {
        sink.windowWidth = size;
        sink.windowHeight = size;
        gst_westeros_sink_soc_update_video_position(&sink);
    }
    sink.windowSizeOverride = FALSE;
}

// ============ Start PTS Boundary Tests ============
TEST_F(WesterosSinkSocRawTest, SocSetStartPTSBoundaryValues) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0LL);
    gst_westeros_sink_soc_set_startPTS(&sink, 1LL);
    gst_westeros_sink_soc_set_startPTS(&sink, -1LL);
    gst_westeros_sink_soc_set_startPTS(&sink, 9223372036854775807LL);  // INT64_MAX
    gst_westeros_sink_soc_set_startPTS(&sink, -9223372036854775807LL); // INT64_MIN+1
}

// ============ Flush Comprehensive ============
TEST_F(WesterosSinkSocRawTest, SocFlushWithDifferentVideoFrameCounts) {
    for (int frameCount = 0; frameCount <= 100; frameCount += 10) {
        sink.soc.frameInCount = frameCount;
        sink.soc.frameOutCount = frameCount / 2;
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocRawTest, SocFlushWithoutInit) {
    // Flush without initialization should be safe
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_flush(&sink);
}

// ============ EOS Event Comprehensive ============
TEST_F(WesterosSinkSocRawTest, SocEosEventWithDifferentPlaybackStates) {
    // Not started
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_eos_event(&sink);
    
    // Started
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_eos_event(&sink);
    
    // With paused flag
    sink.soc.videoPaused = TRUE;
    gst_westeros_sink_soc_eos_event(&sink);
}

// ============ Video Path Management ============
TEST_F(WesterosSinkSocRawTest, SocSetVideoPathGfxAndVideoAlternating) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, (i % 2 == 0));
    }
}

TEST_F(WesterosSinkSocRawTest, SocSetVideoPathWithCaptureEnabled) {
    sink.soc.captureEnabled = TRUE;
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_set_video_path(&sink, false);
    sink.soc.captureEnabled = FALSE;
}

TEST_F(WesterosSinkSocRawTest, SocSetVideoPathWithTunnelled) {
    sink.soc.useTunnelled = TRUE;
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_set_video_path(&sink, false);
    sink.soc.useTunnelled = FALSE;
}

// ============ Query Function Comprehensive ============
TEST_F(WesterosSinkSocRawTest, SocQueryConsistency) {
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    }
}

} // namespace
