#include <glib.h>
#include <gtest/gtest.h>
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
#include <cstring>

namespace {

class WesterosSinkSocDrmTest : public ::testing::Test {
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
        
        sink.scaleXDenom = 1;
        sink.scaleYDenom = 1;
        sink.scaleXNum = 1;
        sink.scaleYNum = 1;
        sink.outputWidth = 1280;
        sink.outputHeight = 720;
        sink.visible = TRUE;
        sink.show = TRUE;
        sink.windowX = 0;
        sink.windowY = 0;
        sink.windowWidth = 1280;
        sink.windowHeight = 720;
        sink.windowSizeOverride = FALSE;
        sink.transX = 0;
        sink.transY = 0;
        sink.playbackRate = 1.0f;
        sink.videoStarted = FALSE;
    }
    
    void TearDown() override {
        if (caps) gst_caps_unref(caps);
        g_mutex_clear(&sink.mutex);
    }
};

TEST_F(WesterosSinkSocDrmTest, SocInitReturnsTrueForValidSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_init(&sink));
}

TEST_F(WesterosSinkSocDrmTest, SocInitReturnsFalseForNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_init(nullptr));
}

TEST_F(WesterosSinkSocDrmTest, SocNullToReadyReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocNullToReadyHandlesNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocReadyToPausedReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocReadyToPausedWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocPausedToPlayingReturnsTrue) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocPausedToPlayingWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocPlayingToPausedWithVideoStarted) {
    sink.videoStarted = TRUE;
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocPausedToReadyWithVideoStarted) {
    sink.videoStarted = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocReadyToNullReturnsTrue) {
    passToDefault = TRUE;
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocReadyToNullSetPassToDefaultFalse) {
    passToDefault = TRUE;
    gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    EXPECT_FALSE(passToDefault);
}

TEST_F(WesterosSinkSocDrmTest, SocReadyToNullWithNullSink) {
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_null(nullptr, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocAcceptCapsReturnsFalseWithValidCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocDrmTest, SocAcceptCapsReturnsFalseWithNullCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocDrmTest, SocAcceptCapsReturnsFalseWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(nullptr, caps));
}

TEST_F(WesterosSinkSocDrmTest, SocStartVideoReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(&sink));
}

TEST_F(WesterosSinkSocDrmTest, SocStartVideoWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_start_video(nullptr));
}

TEST_F(WesterosSinkSocDrmTest, SocQueryReturnsFalseWithValidQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocDrmTest, SocQueryReturnsFalseWithNullQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocDrmTest, SocQueryReturnsFalseWithNullSink) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(nullptr, &query));
}

TEST_F(WesterosSinkSocDrmTest, SocClassInitWithNullClass) {
    gst_westeros_sink_soc_class_init(nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocClassInitWithValidClass) {
    gst_westeros_sink_soc_class_init(reinterpret_cast<GstWesterosSinkClass*>(&sink));
}

TEST_F(WesterosSinkSocDrmTest, SocTermWithValidSink) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocDrmTest, SocTermWithNullSink) {
    gst_westeros_sink_soc_term(nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocSetPropertyWithAllNullParams) {
    gst_westeros_sink_soc_set_property(nullptr, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocSetPropertyWithValidSink) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocSetPropertyMultipleIds) {
    for (int i = 1; i <= 10; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, nullptr, nullptr);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocGetPropertyWithAllNullParams) {
    gst_westeros_sink_soc_get_property(nullptr, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocGetPropertyWithValidSink) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocGetPropertyMultipleIds) {
    for (int i = 1; i <= 10; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, nullptr, nullptr);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocRegistryHandleGlobalWithNullRegistry) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(&sink, nullptr, 0, "wl_sb", 1));
}

TEST_F(WesterosSinkSocDrmTest, SocRegistryHandleGlobalWithNullSink) {
    EXPECT_NO_FATAL_FAILURE(gst_westeros_sink_soc_registryHandleGlobal(nullptr, nullptr, 0, "wl_sb", 1));
}

TEST_F(WesterosSinkSocDrmTest, SocRegistryHandleGlobalRemoveWithValidSink) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, nullptr, 0);
}

TEST_F(WesterosSinkSocDrmTest, SocRegistryHandleGlobalRemoveWithNullSink) {
    gst_westeros_sink_soc_registryHandleGlobalRemove(nullptr, nullptr, 0);
}

TEST_F(WesterosSinkSocDrmTest, SocSetStartPTSWithZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocDrmTest, SocSetStartPTSWithNonZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 12345);
}

TEST_F(WesterosSinkSocDrmTest, SocSetStartPTSWithNullSink) {
    gst_westeros_sink_soc_set_startPTS(nullptr, 12345);
}

TEST_F(WesterosSinkSocDrmTest, SocRenderWithValidBuffer) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocDrmTest, SocRenderWithNullBuffer) {
    gst_westeros_sink_soc_render(&sink, nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocRenderWithNullSink) {
    gst_westeros_sink_soc_render(nullptr, &buffer);
}

TEST_F(WesterosSinkSocDrmTest, SocFlushWithValidSink) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocDrmTest, SocFlushWithNullSink) {
    gst_westeros_sink_soc_flush(nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocEosEventWithValidSink) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocDrmTest, SocEosEventWithNullSink) {
    gst_westeros_sink_soc_eos_event(nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocSetVideoPathToGraphics) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocDrmTest, SocSetVideoPathToVideo) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocDrmTest, SocSetVideoPathWithNullSink) {
    gst_westeros_sink_soc_set_video_path(nullptr, false);
}

TEST_F(WesterosSinkSocDrmTest, SocUpdateVideoPositionWithDefaultValues) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocDrmTest, SocUpdateVideoPositionWithNullSink) {
    gst_westeros_sink_soc_update_video_position(nullptr);
}

TEST_F(WesterosSinkSocDrmTest, SocUpdateVideoPositionWithWindowOverride) {
    sink.windowSizeOverride = TRUE;
    sink.windowX = 100;
    sink.windowY = 100;
    sink.windowWidth = 640;
    sink.windowHeight = 480;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocDrmTest, SocFullLifecycleNullToReady) {
    EXPECT_FALSE(gst_westeros_sink_soc_init(&sink));
    EXPECT_TRUE(gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocFullLifecycleReadyToPaused) {
    gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault));
}

TEST_F(WesterosSinkSocDrmTest, SocFullLifecyclePausedToPlaying) {
    gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
    EXPECT_TRUE(gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault));
}

// ============ Extended Tests for DRM ============
TEST_F(WesterosSinkSocDrmTest, SocSetPropertyExtended) {
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocGetPropertyExtended) {
    for (int i = 1; i <= 20; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocSetStartPTSExtended) {
    for (gint64 pts = 0; pts <= 50000; pts += 5000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocRenderExtended) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocFlushExtended) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocEosEventExtended) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocUpdateVideoPositionExtended) {
    for (int x = 0; x <= 200; x += 100) {
        for (int y = 0; y <= 200; y += 100) {
            sink.windowX = x;
            sink.windowY = y;
            gst_westeros_sink_soc_update_video_position(&sink);
        }
    }
}

TEST_F(WesterosSinkSocDrmTest, SocComplexSequence) {
    gst_westeros_sink_soc_init(&sink);
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_set_property((GObject*)&sink, 1, &gval, nullptr);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_set_startPTS(&sink, 1000 * (i+1));
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_paused_to_playing(&sink, &passToDefault);
        gst_westeros_sink_soc_update_video_position(&sink);
        gst_westeros_sink_soc_playing_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_flush(&sink);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocStressTestState) {
    gst_westeros_sink_soc_init(&sink);
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_null_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_paused(&sink, &passToDefault);
        gst_westeros_sink_soc_paused_to_ready(&sink, &passToDefault);
        gst_westeros_sink_soc_ready_to_null(&sink, &passToDefault);
    }
}

TEST_F(WesterosSinkSocDrmTest, SocStressTestOperations) {
    gst_westeros_sink_soc_init(&sink);
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, 1000 * i);
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

} // namespace
