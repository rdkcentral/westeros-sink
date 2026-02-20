#include <glib.h>
#include <gtest/gtest.h>
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
#include <cstring>

namespace {

class WesterosSinkSocBrcmTest : public ::testing::Test {
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
        
        // Initialize mutex for thread safety 
        g_mutex_init(&sink.mutex);
        
        // Initialize scale denominators to prevent division by zero
        sink.scaleXDenom = 1;
        sink.scaleYDenom = 1;
        sink.scaleXNum = 1;
        sink.scaleYNum = 1;
        
        // Initialize output dimensions
        sink.outputWidth = 1280;
        sink.outputHeight = 720;
        
        // Initialize visibility and window properties
        sink.visible = TRUE;
        sink.show = TRUE;
        sink.windowX = 0;
        sink.windowY = 0;
        sink.windowWidth = 1280;
        sink.windowHeight = 720;
        sink.windowSizeOverride = FALSE;
        
        // Initialize transform properties
        sink.transX = 0;
        sink.transY = 0;
        
        // Initialize playback properties
        sink.playbackRate = 1.0f;
        sink.videoStarted = FALSE;
        
        // Initialize SOC-specific fields
        sink.soc.videoX = 0;
        sink.soc.videoY = 0;
        sink.soc.videoWidth = 1280;
        sink.soc.videoHeight = 720;
        sink.soc.videoWindow = nullptr;
        sink.soc.videoDecoder = nullptr;
        sink.soc.surfaceClient = nullptr;
        sink.soc.surfaceClientId = 0;
        sink.soc.sb = nullptr;
        sink.soc.captureEnabled = FALSE;
        sink.soc.usePip = FALSE;
        sink.soc.ptsOffset = 0;
        sink.soc.lastStartPts45k = 0;
        sink.soc.chkBufToStartPts = FALSE;
        sink.soc.lastRenderPts = 0;
        sink.soc.outputFormat = NEXUS_VideoFormat_e720p;
        sink.soc.frameCount = 0;
        sink.soc.presentationStarted = FALSE;
        sink.soc.useImmediateOutput = FALSE;
        sink.soc.useLowDelay = FALSE;
        sink.soc.latencyTarget = 100;
        sink.soc.serverPlaySpeed = 1.0;
        sink.soc.clientPlaySpeed = 1.0;
        sink.soc.clientPlaying = FALSE;
        sink.soc.videoPlaying = FALSE;
        sink.soc.zoomMode = NEXUS_VideoWindowContentMode_eFull;
        sink.soc.zoomSet = FALSE;
        sink.soc.forceAspectRatio = FALSE;
        sink.soc.stcChannel = nullptr;
    }
    
    void TearDown() override {
        if (caps) gst_caps_unref(caps);
        g_mutex_clear(&sink.mutex);
    }
};

// ============ Initialization Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocInitReturnsTrueForValidSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocBrcmTest, SocInitReturnsFalseForNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_init(nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocInitMultipleTimes) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

// ============ State Transition Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocNullToReadyReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocNullToReadyHandlesNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocNullToReadyHandlesNullPassToDefault) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToPausedReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToPausedWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToPausedWithNullPassToDefault) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocPausedToPlayingReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPausedToPlayingWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPlayingToPausedWithVideoStarted) {
    sink.videoStarted = TRUE;
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPlayingToPausedWithoutVideoStarted) {
    sink.videoStarted = FALSE;
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPlayingToPausedWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPausedToReadyWithVideoStarted) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocPausedToReadyWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToNullReturnsTrue) {
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToNullSetPassToDefaultFalse) {
    passToDefault = TRUE;
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    EXPECT_FALSE(passToDefault);
}

TEST_F(WesterosSinkSocBrcmTest, SocReadyToNullWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(nullptr, &passToDefault));
}

// ============ Capability Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsReturnsFalseWithValidCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsReturnsFalseWithNullCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsReturnsFalseWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(nullptr, caps));
}

TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsMultipleCalls) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

// ============ Video and Query Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocStartVideoReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocBrcmTest, SocStartVideoWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocQueryReturnsFalseWithValidQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocBrcmTest, SocQueryReturnsFalseWithNullQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocBrcmTest, SocQueryReturnsFalseWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(nullptr, &query));
}

// ============ Class Initialization Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocClassInitWithNullClass) {
    gst_westeros_sink_soc_class_init(nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocClassInitWithValidClass) {
    gst_westeros_sink_soc_class_init(reinterpret_cast<GstWesterosSinkClass*>(&sink));
}

// ============ Sink Lifecycle Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocTermWithValidSink) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocTermWithNullSink) {
    gst_westeros_sink_soc_term(nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocTermMultipleTimes) {
    gst_westeros_sink_soc_term(&sink);
    gst_westeros_sink_soc_term(&sink);
}

// ============ Property Tests - Set Property ============
TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyWithAllNullParams) {
    gst_westeros_sink_soc_set_property(nullptr, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyWithValidSinkNullValue) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyWithValidSinkValidValue) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyMultipleProperties) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 2, nullptr, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 3, nullptr, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 5, nullptr, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 10, nullptr, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 15, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyVariousIds) {
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyWithNullValue) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 5, nullptr, nullptr);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 10, nullptr, nullptr);
}

// ============ Property Tests - Get Property ============
TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyWithAllNullParams) {
    gst_westeros_sink_soc_get_property(nullptr, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyWithValidSinkNullValue) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyWithValidSinkValidValue) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyMultipleProperties) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 2, nullptr, nullptr);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 3, nullptr, nullptr);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 8, nullptr, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyVariousIds) {
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gval, nullptr);
    }
}

// ============ Registry and Event Handler Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalWithNullRegistry) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", 1));
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalWithOtherInterface) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "other", 1));
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalWithNullSink) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(nullptr, nullptr, 0, "wl_sb", 1));
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalMultipleInterfaces) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", 1));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "interface1", 1));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "interface2", 2));
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalRemoveWithValidSink) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 0);
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalRemoveWithNullSink) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(nullptr, nullptr, 0);
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalRemoveMultipleTimes) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 0);
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 1);
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 2);
}

// ============ PTS Management Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSWithZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSWithNonZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 12345);
    EXPECT_EQ(sink.soc.lastStartPts45k, 12345 / 2);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSWithNullSink) {
    gst_westeros_sink_soc_set_startPTS(nullptr, 12345);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSMultipleCalls) {
    gst_westeros_sink_soc_set_startPTS(&sink, 1000);
    gst_westeros_sink_soc_set_startPTS(&sink, 2000);
    gst_westeros_sink_soc_set_startPTS(&sink, 2000);  
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSLargePts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 1000000);
    EXPECT_EQ(sink.soc.lastStartPts45k, 1000000 / 2);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSVariousValues) {
    for (gint64 pts = 0; pts <= 10000; pts += 1000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

// ============ Buffer and Rendering Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocRenderWithValidBuffer) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderWithNullBuffer) {
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderWithNullSink) {
    gst_westeros_sink_soc_render(nullptr, &buffer);
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderMultipleTimes) {
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderWithVariousStates) {
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_render(&sink, &buffer);
}

// ============ Flush Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocFlushWithValidSink) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocFlushWithNullSink) {
    gst_westeros_sink_soc_flush(nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocFlushMultipleTimes) {
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocFlushWithVideoStarted) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_flush(&sink);
}

// ============ End-of-Stream Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocEosEventWithValidSink) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocEosEventWithNullSink) {
    gst_westeros_sink_soc_eos_event(nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocEosEventAfterVideoStarted) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocEosEventMultipleTimes) {
    gst_westeros_sink_soc_eos_event(&sink);
    gst_westeros_sink_soc_eos_event(&sink);
}

// ============ Video Path Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathToGraphics) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathToVideo) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathWithNullSink) {
    gst_westeros_sink_soc_set_video_path(nullptr, false);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathToggle) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_set_video_path(&sink, false);
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, (i % 2 == 0));
    }
}

// ============ Video Position Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithDefaultValues) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithNullSink) {
    gst_westeros_sink_soc_update_video_position(nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithWindowOverride) {
    sink.windowSizeOverride = TRUE;
    sink.windowX = 100;
    sink.windowY = 100;
    sink.windowWidth = 640;
    sink.windowHeight = 480;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithoutWindowOverride) {
    sink.windowSizeOverride = FALSE;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionMultipleTimes) {
    gst_westeros_sink_soc_update_video_position(&sink);
    sink.windowX = 50;
    sink.windowY = 50;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionChangeVisibility) {
    sink.visible = TRUE;
    gst_westeros_sink_soc_update_video_position(&sink);
    sink.visible = FALSE;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionVariousPositions) {
    for (int x = 0; x < 640; x += 100) {
        for (int y = 0; y < 480; y += 100) {
            sink.windowX = x;
            sink.windowY = y;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionVariousDimensions) {
    sink.windowWidth = 320;
    sink.windowHeight = 240;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.windowWidth = 640;
    sink.windowHeight = 480;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.windowWidth = 1920;
    sink.windowHeight = 1080;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithTransforms) {
    sink.transX = 10;
    sink.transY = 20;
    gst_westeros_sink_soc_update_video_position(&sink);
}

// ============ Full Lifecycle Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocFullLifecycleNullToReady) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecycleReadyToPaused) {
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecyclePausedToPlaying) {
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecyclePlayingToPaused) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecycleBackToReady) {
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecycleToNull) {
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocBrcmTest, SocFullLifecycleWithBuffers) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_update_video_position(&sink);
    gst_westeros_sink_soc_flush(&sink);
    
    gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
}

// ============ Combined State and Property Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocStateTransitionWithPropertyChanges) {
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_set_property((GObject*)&sink, 3, &gval, nullptr);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_get_property((GObject*)&sink, 3, &gval, nullptr);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
}

TEST_F(WesterosSinkSocBrcmTest, SocPropertySetGetCycle) {
    for (int i = 1; i <= 10; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gval, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocStateWithVideoOperations) {
    gst_westeros_sink_soc_init(&sink);
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    gst_westeros_sink_soc_set_startPTS(&sink, 5000);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_eos_event(&sink);
}

// ============ Extended Property Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyVisibility) {
    sink.visible = FALSE;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gval, nullptr);
    sink.visible = TRUE;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyWindowPosition) {
    sink.windowX = 0;
    sink.windowY = 0;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 4, &gval, nullptr);
    
    sink.windowX = 100;
    sink.windowY = 200;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 4, &gval, nullptr);
    
    sink.windowX = 1920;
    sink.windowY = 1080;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 4, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyScale) {
    sink.scaleXNum = 1;
    sink.scaleXDenom = 1;
    sink.scaleYNum = 1;
    sink.scaleYDenom = 1;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 6, &gval, nullptr);
    
    sink.scaleXNum = 2;
    sink.scaleXDenom = 3;
    sink.scaleYNum = 3;
    sink.scaleYDenom = 4;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 6, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetPropertyPlaybackRate) {
    sink.playbackRate = 0.5f;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 7, &gval, nullptr);
    
    sink.playbackRate = 1.0f;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 7, &gval, nullptr);
    
    sink.playbackRate = 2.0f;
    gst_westeros_sink_soc_set_property((GObject*)&sink, 7, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyVisibility) {
    sink.visible = FALSE;
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &gval, nullptr);
    
    sink.visible = TRUE;
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &gval, nullptr);
}

TEST_F(WesterosSinkSocBrcmTest, SocGetPropertyWindowPosition) {
    sink.windowX = 100;
    sink.windowY = 200;
    gst_westeros_sink_soc_get_property((GObject*)&sink, 4, &gval, nullptr);
    
    sink.windowX = 1280;
    sink.windowY = 720;
    gst_westeros_sink_soc_get_property((GObject*)&sink, 4, &gval, nullptr);
}

// ============ Extended State Transition Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocComplexStateSequence1) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
}

TEST_F(WesterosSinkSocBrcmTest, SocComplexStateSequence2) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
}

TEST_F(WesterosSinkSocBrcmTest, SocComplexStateSequence3) {
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
}

// ============ Extended PTS Management Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSBoundaries) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
    gst_westeros_sink_soc_set_startPTS(&sink, 1);
    gst_westeros_sink_soc_set_startPTS(&sink, 255);
    gst_westeros_sink_soc_set_startPTS(&sink, 256);
    gst_westeros_sink_soc_set_startPTS(&sink, 65535);
    gst_westeros_sink_soc_set_startPTS(&sink, 65536);
    gst_westeros_sink_soc_set_startPTS(&sink, 1048575);
    gst_westeros_sink_soc_set_startPTS(&sink, 1048576);
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSSequential) {
    for (gint64 pts = 0; pts <= 50000; pts += 5000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocSetStartPTSWithStateChanges) {
    gst_westeros_sink_soc_set_startPTS(&sink, 1000);
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_set_startPTS(&sink, 2000);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_set_startPTS(&sink, 3000);
}

// ============ Extended Registry Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalVariousInterfaces) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "", 1));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", 1));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", 2));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "interface1", 1));
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "interface_long_name_test", 1));
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalWithDifferentVersions) {
    for (uint32_t version = 1; version <= 10; version++) {
        EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", version));
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocRegistryHandleGlobalRemoveMultipleVersions) {
    for (uint32_t id = 0; id < 20; id++) {
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, id);
    }
}

// ============ Extended Video Rendering Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocRenderSequence) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderAfterStateTransitions) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocBrcmTest, SocRenderWithDifferentVideoStates) {
    sink.videoStarted = FALSE;
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, nullptr);
    
    sink.videoStarted = TRUE;
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, nullptr);
}

// ============ Extended Flush Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocFlushSequence) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocFlushInterleaved) {
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
}

// ============ Extended End-of-Stream Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocEosEventSequence) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocEosEventAfterStateTransitions) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_eos_event(&sink);
    
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_eos_event(&sink);
    
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_eos_event(&sink);
}

// ============ Extended Video Path Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathAlternating) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocSetVideoPathWithRender) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_set_video_path(&sink, false);
    gst_westeros_sink_soc_render(&sink, &buffer);
    
    gst_westeros_sink_soc_set_video_path(&sink, true);
    gst_westeros_sink_soc_render(&sink, &buffer);
}

// ============ Extended Video Position Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionBoundaries) {
    sink.windowX = 0;
    sink.windowY = 0;
    sink.windowWidth = 1;
    sink.windowHeight = 1;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.windowX = 2160;
    sink.windowY = 2160;
    sink.windowWidth = 4320;
    sink.windowHeight = 2160;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionCombinations) {
    for (int x = 0; x <= 200; x += 100) {
        for (int y = 0; y <= 200; y += 100) {
            sink.windowX = x;
            sink.windowY = y;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocUpdateVideoPositionWithScale) {
    sink.scaleXNum = 1;
    sink.scaleXDenom = 1;
    sink.scaleYNum = 1;
    sink.scaleYDenom = 1;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.scaleXNum = 1;
    sink.scaleXDenom = 2;
    sink.scaleYNum = 1;
    sink.scaleYDenom = 2;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.scaleXNum = 2;
    sink.scaleXDenom = 1;
    sink.scaleYNum = 2;
    sink.scaleYDenom = 1;
    gst_westeros_sink_soc_update_video_position(&sink);
}

// ============ Comprehensive Query Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocQueryReturnsFalseWithVariousQueries) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    
    EXPECT_FALSE(gst_westeros_sink_soc_query(nullptr, &query));
}

TEST_F(WesterosSinkSocBrcmTest, SocQueryAfterStateTransitions) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
    
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

// ============ Caps Acceptance Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsMultipleCapsVariations) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocBrcmTest, SocAcceptCapsAfterInit) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

// ============ Video Start Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocStartVideoReturnsFalseRepeatedly) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocBrcmTest, SocStartVideoAfterInit) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
}

// ============ Class Initialization Extended Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocClassInitMultipleCalls) {
    gst_westeros_sink_soc_class_init(reinterpret_cast<GstWesterosSinkClass*>(&sink));
    gst_westeros_sink_soc_class_init(reinterpret_cast<GstWesterosSinkClass*>(&sink));
    gst_westeros_sink_soc_class_init(reinterpret_cast<GstWesterosSinkClass*>(&sink));
}

// ============ Initialization Extended Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocInitSequential) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocBrcmTest, SocInitWithDifferentStates) {
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
    
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_init(&sink));
}

// ============ Term Extended Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocTermSequential) {
    gst_westeros_sink_soc_term(&sink);
    gst_westeros_sink_soc_term(&sink);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocTermAfterStateTransitions) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
    
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    gst_westeros_sink_soc_term(&sink);
}

// ============ Comprehensive Combined Operation Tests ============
TEST_F(WesterosSinkSocBrcmTest, SocComprehensiveOperationSequence) {
    gst_westeros_sink_soc_init(&sink);
    
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gval, nullptr);
        gst_westeros_sink_soc_get_property((GObject*)&sink, 1, &gval, nullptr);
        
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_set_startPTS(&sink, 1000 * (i+1));
        gst_westeros_sink_soc_render(&sink, &buffer);
        
        gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
        gst_westeros_sink_soc_update_video_position(&sink);
        
        gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_flush(&sink);
        gst_westeros_sink_soc_eos_event(&sink);
        
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
    
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocBrcmTest, SocStressTestPropertySetGet) {
    for (int propId = 1; propId <= 20; propId++) {
        for (int i = 0; i < 3; i++) {
            gst_westeros_sink_soc_set_property((GObject*)&sink, propId, &gval, nullptr);
            gst_westeros_sink_soc_get_property((GObject*)&sink, propId, &gval, nullptr);
        }
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocStressTestStateTransitions) {
    gst_westeros_sink_soc_init(&sink);
    
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
}

TEST_F(WesterosSinkSocBrcmTest, SocStressTestVideoOperations) {
    gst_westeros_sink_soc_init(&sink);
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    
    for (int i = 0; i < 20; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, 1000 * i);
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_update_video_position(&sink);
        gst_westeros_sink_soc_set_video_path(&sink, i % 2 == 0);
    }
    
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
}

} // namespace
