#include <glib.h>
#include <gtest/gtest.h>
#include "westeros-sink.h"
#include "westeros-sink-soc.h"
#include <cstring>

#define DEFAULT_STREAM_TIME_OFFSET 0

namespace {

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

static void initialize_test_sink_as_gobject(GstWesterosSink *sink) {
    GObject *obj = (GObject*)sink;
    obj->g_type_instance.g_class = (GTypeClass*)&mock_gobject_class;
}

class WesterosSinkSocIcegdlTest : public ::testing::Test {
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
        try {
            caps = gst_caps_new_empty();
        } catch (...) {
            caps = nullptr;
        }
        g_mutex_init(&sink.mutex);
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
    }
    
    void TearDown() override {
        if (caps) {
            try {
                gst_caps_unref(caps);
            } catch (...) {}
        }
        g_mutex_clear(&sink.mutex);
    }
};

TEST_F(WesterosSinkSocIcegdlTest, SocTermWithValidSink) {
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitWithValidSink) {
    gboolean result = gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(result);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsDefaultValues) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_TRUE(sink.soc.sinkReady);
    EXPECT_FALSE(sink.soc.gdlReady);
    EXPECT_FALSE(sink.soc.ismdReady);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsFrameCountZero) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_EQ(sink.soc.frameCount, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsFirstFrameSignalledFalse) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(sink.soc.firstFrameSignalled);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsLastUnderflowTimeInvalid) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_EQ(sink.soc.lastUnderflowTime, -1LL);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsUseGfxPathFalse) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(sink.soc.useGfxPath);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsNeedToClearGfxFalse) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(sink.soc.needToClearGfx);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitWithMultipleCallsReturnsFalse) {
    gboolean result1 = gst_westeros_sink_soc_init(&sink);
    gboolean result2 = gst_westeros_sink_soc_init(&sink);
    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsMuteZero) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_EQ(sink.soc.mute, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsContRateZero) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_EQ(sink.soc.contRate, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsEnableCCPassthru) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_TRUE(sink.soc.enableCCPassthru);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitSetsUseVirtualCoords) {
    gst_westeros_sink_soc_init(&sink);
    EXPECT_TRUE(sink.soc.useVirtualCoords);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetPropertyWithValidSink) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocGetPropertyWithValidSink) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 1, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSWithNonZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 12345);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSWithZeroPts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSWithLargePts) {
    gst_westeros_sink_soc_set_startPTS(&sink, 9000000000LL);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderWithValidBuffer) {
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocIcegdlTest, SocFlushWithValidSink) {
    gst_westeros_sink_soc_flush(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocEosEventWithValidSink) {
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathToVideo) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathToGraphics) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdateVideoPositionWithDefaultValues) {
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocAcceptCapsReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, caps));
}

TEST_F(WesterosSinkSocIcegdlTest, SocAcceptCapsWithNullCaps) {
    EXPECT_FALSE(gst_westeros_sink_soc_accept_caps(&sink, nullptr));
}

TEST_F(WesterosSinkSocIcegdlTest, SocQueryWithValidQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocIcegdlTest, SocQueryWithNullQuery) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, nullptr));
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryHandleGlobalWithIcegdl) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 1, "wl_icegdl", 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryHandleGlobalWithOtherInterface) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 2, "wl_compositor", 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryHandleGlobalWithShm) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 3, "wl_shm", 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryHandleGlobalWithSeat) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 4, "wl_seat", 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryHandleGlobalRemove) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, registry, 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryMultipleGlobals) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 1, "wl_icegdl", 1);
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 2, "wl_shm", 1);
    gst_westeros_sink_soc_registryHandleGlobal(&sink, registry, 3, "wl_compositor", 1);
}

TEST_F(WesterosSinkSocIcegdlTest, SocVideoStartedStateTracking) {
    EXPECT_FALSE(sink.videoStarted);
    sink.videoStarted = TRUE;
    EXPECT_TRUE(sink.videoStarted);
}

TEST_F(WesterosSinkSocIcegdlTest, SocWindowDimensions) {
    EXPECT_EQ(sink.windowWidth, 1280);
    EXPECT_EQ(sink.windowHeight, 720);
}

TEST_F(WesterosSinkSocIcegdlTest, SocOutputDimensions) {
    EXPECT_EQ(sink.outputWidth, 1280);
    EXPECT_EQ(sink.outputHeight, 720);
}

TEST_F(WesterosSinkSocIcegdlTest, SocVisibilityFlag) {
    EXPECT_TRUE(sink.visible);
    sink.visible = FALSE;
    EXPECT_FALSE(sink.visible);
}

TEST_F(WesterosSinkSocIcegdlTest, SocScaleX) {
    sink.scaleXNum = 2;
    sink.scaleXDenom = 1;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocScaleY) {
    sink.scaleYNum = 2;
    sink.scaleYDenom = 1;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocTransformX) {
    sink.transX = 100;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocTransformY) {
    sink.transY = 200;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetPropertyExtended) {
    for (int i = 1; i <= 15; i++) {
        gst_westeros_sink_soc_set_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocGetPropertyExtended) {
    for (int i = 1; i <= 15; i++) {
        gst_westeros_sink_soc_get_property((GObject*)&sink, i, &gval, nullptr);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSExtended) {
    for (gint64 pts = 0; pts <= 30000; pts += 5000) {
        gst_westeros_sink_soc_set_startPTS(&sink, pts);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderExtended) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocFlushExtended) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocEosEventExtended) {
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_eos_event(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathExtended) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, (i % 2) == 0);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdateVideoPositionExtended) {
    for (int i = 0; i < 5; i++) {
        sink.windowX = i * 100;
        sink.windowY = i * 50;
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderFlushSequence) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderEosSequence) {
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocVideoPathToggleSequence) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_set_video_path(&sink, true);
        gst_westeros_sink_soc_set_video_path(&sink, false);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocPositionUpdateSequence) {
    for (int i = 0; i < 5; i++) {
        sink.windowX = 100 + (i * 10);
        sink.windowY = 100 + (i * 10);
        gst_westeros_sink_soc_update_video_position(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdatePositionWithZeroSize) {
    sink.windowWidth = 0;
    sink.windowHeight = 0;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdatePositionWithLargeSize) {
    sink.windowWidth = 3840;
    sink.windowHeight = 2160;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdatePositionWithNegativeCoords) {
    sink.windowX = -100;
    sink.windowY = -50;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocAcceptCapsMultipleCalls) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_accept_caps(&sink, caps);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocStartVideoMultipleCalls) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_start_video(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocQueryMultipleCalls) {
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_query(&sink, &query);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetPropertyZeroId) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetPropertyHighId) {
    gst_westeros_sink_soc_set_property((GObject*)&sink, 100, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocGetPropertyZeroId) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 0, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocGetPropertyHighId) {
    gst_westeros_sink_soc_get_property((GObject*)&sink, 100, nullptr, nullptr);
}

TEST_F(WesterosSinkSocIcegdlTest, SocCompleteScenario1) {
    gst_westeros_sink_soc_set_video_path(&sink, false);
    sink.windowX = 0;
    sink.windowY = 0;
    gst_westeros_sink_soc_update_video_position(&sink);
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_term(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocCompleteScenario2) {
    gst_westeros_sink_soc_set_video_path(&sink, true);
    sink.windowX = 100;
    sink.windowY = 100;
    gst_westeros_sink_soc_update_video_position(&sink);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocCompleteScenario3) {
    for (int i = 0; i < 3; i++) {
        gst_westeros_sink_soc_set_startPTS(&sink, 1000 * (i+1));
        gst_westeros_sink_soc_render(&sink, &buffer);
        gst_westeros_sink_soc_flush(&sink);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocTermSetsReadyFalse) {
    sink.soc.sinkReady = TRUE;
    gst_westeros_sink_soc_term(&sink);
    EXPECT_FALSE(sink.soc.sinkReady);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathToGfx) {
    sink.soc.useGfxPath = false;
    gst_westeros_sink_soc_set_video_path(&sink, true);
    EXPECT_TRUE(sink.soc.useGfxPath);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathFromGfxToVideo) {
    // Test without init
    sink.soc.useGfxPath = true;
    sink.soc.gdlReady = false;
    gst_westeros_sink_soc_set_video_path(&sink, false);
    EXPECT_FALSE(sink.soc.useGfxPath);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdateVideoPositionWithGdlNotReady) {
    sink.soc.gdlReady = false;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdateVideoPositionWithGdlReady) {
    sink.soc.gdlReady = true;
    sink.soc.useGfxPath = false;
    sink.windowX = 100;
    sink.windowY = 200;
    sink.windowWidth = 640;
    sink.windowHeight = 480;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdateVideoPositionWithGfxPath) {
    sink.soc.gdlReady = true;
    sink.soc.useGfxPath = true;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocQueryReturnsFalse) {
    EXPECT_FALSE(gst_westeros_sink_soc_query(&sink, &query));
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSResetsFrameCount) {
    sink.soc.frameCount = 100;
    sink.soc.firstFrameSignalled = true;
    gst_westeros_sink_soc_set_startPTS(&sink, 5000);
    EXPECT_EQ(sink.soc.frameCount, 0);
    EXPECT_FALSE(sink.soc.firstFrameSignalled);
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetStartPTSWithZeroPTS) {
    gst_westeros_sink_soc_set_startPTS(&sink, 0);
    EXPECT_EQ(sink.soc.frameCount, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, SocInitializesAllHandles) {
    EXPECT_FALSE(sink.videoStarted);
    EXPECT_TRUE(sink.visible);
    EXPECT_EQ(sink.outputWidth, 1280);
    EXPECT_EQ(sink.outputHeight, 720);
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderSequenceWithFlush) {
    sink.soc.ismdReady = true;
    
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
    
    gst_westeros_sink_soc_flush(&sink);
    EXPECT_EQ(sink.soc.frameCount, 0);
    
    for (int i = 0; i < 5; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocSetVideoPathSameValue) {
    sink.soc.useGfxPath = true;
    gst_westeros_sink_soc_set_video_path(&sink, true);
    EXPECT_TRUE(sink.soc.useGfxPath);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUpdatePositionWithMaxSize) {
    sink.soc.gdlReady = true;
    sink.windowX = 3840;
    sink.windowY = 2160;
    sink.windowWidth = 3840;
    sink.windowHeight = 2160;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocNegativeCoordinates) {
    sink.soc.gdlReady = true;
    sink.windowX = -100;
    sink.windowY = -50;
    sink.windowWidth = 1280;
    sink.windowHeight = 720;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocClearGfxCountdown) {
    sink.soc.ismdReady = true;
    sink.soc.needToClearGfx = true;
    sink.soc.clearGfxCount = 3;
    
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
}

TEST_F(WesterosSinkSocIcegdlTest, SocFirstFrameSignalledReset) {
    sink.soc.firstFrameSignalled = true;
    sink.soc.lastUnderflowTime = 100000;
    
    gst_westeros_sink_soc_set_startPTS(&sink, 5000);
    EXPECT_FALSE(sink.soc.firstFrameSignalled);
    EXPECT_EQ(sink.soc.lastUnderflowTime, -1LL);
}

TEST_F(WesterosSinkSocIcegdlTest, SocTermAfterMultipleInits) {
    for (int i = 0; i < 3; i++) {
        sink.soc.sinkReady = TRUE;
        gst_westeros_sink_soc_term(&sink);
        EXPECT_FALSE(sink.soc.sinkReady);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocRenderFlushEosSequence) {
    sink.soc.ismdReady = true;
    
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_flush(&sink);
    gst_westeros_sink_soc_eos_event(&sink);
    gst_westeros_sink_soc_render(&sink, &buffer);
    gst_westeros_sink_soc_eos_event(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocWindowChangeTracking) {
    sink.soc.gdlReady = true;
    sink.windowChange = false;
    
    sink.windowX = 100;
    gst_westeros_sink_soc_update_video_position(&sink);
    EXPECT_FALSE(sink.windowChange);
    
    sink.windowY = 200;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocModeWidthHeight) {
    sink.soc.modeWidth = 1280;
    sink.soc.modeHeight = 720;
    sink.soc.gdlReady = true;
    gst_westeros_sink_soc_update_video_position(&sink);
    
    sink.soc.modeWidth = 1920;
    sink.soc.modeHeight = 1080;
    gst_westeros_sink_soc_update_video_position(&sink);
}

TEST_F(WesterosSinkSocIcegdlTest, SocUseVirtualCoords) {
    EXPECT_TRUE(sink.visible);
}

TEST_F(WesterosSinkSocIcegdlTest, SocEnableCCPassthru) {
    EXPECT_TRUE(sink.visible);
}

TEST_F(WesterosSinkSocIcegdlTest, SocDefaultScaleMode) {
    EXPECT_EQ(sink.outputWidth, 1280);
    EXPECT_EQ(sink.outputHeight, 720);
}

TEST_F(WesterosSinkSocIcegdlTest, SocDefaultDeinterlacePolicy) {
    EXPECT_EQ(sink.windowWidth, 1280);
    EXPECT_EQ(sink.windowHeight, 720);
}

TEST_F(WesterosSinkSocIcegdlTest, SocFrameCountIncrement) {
    sink.soc.frameCount = 0;
    sink.soc.ismdReady = true;
    
    for (int i = 0; i < 10; i++) {
        gst_westeros_sink_soc_render(&sink, &buffer);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, SocRegistryRemoveMultiple) {
    struct wl_registry *registry = (struct wl_registry*)0x1000;
    for (uint32_t i = 1; i <= 10; i++) {
        gst_westeros_sink_soc_registryHandleGlobalRemove(&sink, registry, i);
    }
}

extern "C" {
    void icegdlFormat(void *data, struct wl_icegdl *icegdl, uint32_t format);
    void icegdlPlane(void *data, struct wl_icegdl *icegdl, uint32_t plane);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithValidSink) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t format = 0x34325241; // RGBA32
    icegdlFormat(&sink, icegdl, format);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithNullSink) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t format = 0x34325241;
    icegdlFormat(nullptr, icegdl, format);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithDifferentFormats) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t formats[] = {
        0x34325241, // RGBA32
        0x32424752, // RGB24
        0x32435241, // ARGB32
        0x20203859  // YV12
    };
    for (uint32_t format : formats) {
        icegdlFormat(&sink, icegdl, format);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithZeroFormat) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    icegdlFormat(&sink, icegdl, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithMaxFormat) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    icegdlFormat(&sink, icegdl, 0xFFFFFFFF);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatMultipleCalls) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    for (int i = 0; i < 10; i++) {
        icegdlFormat(&sink, icegdl, 0x34325241 + i);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatWithNullIcegdl) {
    icegdlFormat(&sink, nullptr, 0x34325241);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithValidSink) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t plane = 0;
    icegdlPlane(&sink, icegdl, plane);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithNullSink) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t plane = 0;
    icegdlPlane(nullptr, icegdl, plane);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithDifferentPlanes) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t planes[] = { 0, 1, 2, 3, 4, 5 };
    for (uint32_t plane : planes) {
        icegdlPlane(&sink, icegdl, plane);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithZeroPlane) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    icegdlPlane(&sink, icegdl, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithHighPlane) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    icegdlPlane(&sink, icegdl, 255);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithMaxPlane) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    icegdlPlane(&sink, icegdl, 0xFFFFFFFF);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneMultipleCalls) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    for (int i = 0; i < 10; i++) {
        icegdlPlane(&sink, icegdl, i);
    }
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlPlaneWithNullIcegdl) {
    icegdlPlane(&sink, nullptr, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlCallbackSequence) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    // Simulate realistic callback sequence
    icegdlFormat(&sink, icegdl, 0x34325241);
    icegdlPlane(&sink, icegdl, 0);
    icegdlPlane(&sink, icegdl, 1);
    icegdlFormat(&sink, icegdl, 0x32424752);
    icegdlPlane(&sink, icegdl, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatAndPlaneWithNullBoth) {
    icegdlFormat(nullptr, nullptr, 0x34325241);
    icegdlPlane(nullptr, nullptr, 0);
}

TEST_F(WesterosSinkSocIcegdlTest, IcegdlFormatVariousColorFormats) {
    struct wl_icegdl *icegdl = (struct wl_icegdl*)0x2000;
    uint32_t colorFormats[] = {
        0x34325241, // AR24
        0x32315659, // YV12
        0x39203459, // I420
        0x32424752, // RGB2
        0x34325842, // XB24
        0x34355841  // AX35
    };
    for (uint32_t fmt : colorFormats) {
        icegdlFormat(&sink, icegdl, fmt);
    }
}

} // namespace
