#include <gtest/gtest.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
extern "C" {
#include "westeros-sink.h"
#include "sink_backend_mock.h"
}

namespace {

GstPad *createSourcePad()
{
    static GstStaticPadTemplate sourcePadTemplate = GST_STATIC_PAD_TEMPLATE(
        "source", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-raw,format=(string)NV12"));
    return gst_pad_new_from_static_template(&sourcePadTemplate, "source");
}

gboolean handlePeerPositionQuery(GstPad *, GstObject *, GstQuery *query)
{
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_POSITION:
        gst_query_set_position(query, GST_FORMAT_TIME, 11 * GST_SECOND);
        return TRUE;
    case GST_QUERY_DURATION:
        gst_query_set_duration(query, GST_FORMAT_TIME, 42 * GST_SECOND);
        return TRUE;
    case GST_QUERY_SEEKING:
        gst_query_set_seeking(query, GST_FORMAT_TIME, TRUE, 0, 42 * GST_SECOND);
        return TRUE;
    case GST_QUERY_CUSTOM:
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

struct TimeCodeSignalState {
    int calls = 0;
    guint hours = 0;
    guint minutes = 0;
    guint seconds = 0;
};

void recordTimeCodeSignal(GstWesterosSink *, guint hours, guint minutes,
                          guint seconds, gpointer userData)
{
    auto *state = static_cast<TimeCodeSignalState *>(userData);
    ++state->calls;
    state->hours = hours;
    state->minutes = minutes;
    state->seconds = seconds;
}

gboolean interceptPadEvent(GstWesterosSink *, GstPad *, GstEvent *, gboolean *passToDefault)
{
    *passToDefault = FALSE;
    return TRUE;
}

struct SendEventState {
    int calls = 0;
    gboolean result = TRUE;
};

SendEventState sendEventState;

gboolean interceptElementEvent(GstWesterosSink *, GstEvent *event, gboolean *passToDefault)
{
    ++sendEventState.calls;
    *passToDefault = FALSE;
    gst_event_unref(event);
    return sendEventState.result;
}

class WesterosSinkTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { gst_init(nullptr, nullptr); }

    void SetUp() override
    {
        sink_backend_mock_reset();
        sink = GST_WESTEROS_SINK(g_object_new(GST_TYPE_WESTEROS_SINK, nullptr));
        ASSERT_NE(nullptr, sink);
    }

    void TearDown() override
    {
        if (sink) {
            gst_object_unref(sink);
            sink = nullptr;
        }
    }

    GstWesterosSink *sink = nullptr;
};

TEST_F(WesterosSinkTest, InitializesGstreamerElementWithRawVideoPadAndDefaults)
{
    EXPECT_EQ(1, sink_backend_mock_state().initCalls);
    EXPECT_EQ(0, sink->windowX);
    EXPECT_EQ(0, sink->windowY);
    EXPECT_EQ(1280, sink->windowWidth);
    EXPECT_EQ(720, sink->windowHeight);
    EXPECT_DOUBLE_EQ(1.0, sink->playbackRate);
    EXPECT_FLOAT_EQ(1.0f, sink->opacity);
    EXPECT_FLOAT_EQ(0.0f, sink->zorder);

    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    GstCaps *caps = gst_pad_get_pad_template_caps(pad);
    ASSERT_NE(nullptr, caps);
    GstCaps *rawCaps = gst_caps_from_string("video/x-raw,format=(string)NV12");
    ASSERT_NE(nullptr, rawCaps);
    EXPECT_TRUE(gst_caps_can_intersect(caps, rawCaps));
    gst_caps_unref(rawCaps);
    gst_caps_unref(caps);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, WindowRectanglePropertyParsesAndTracksOverride)
{
    g_object_set(sink, "window_set", "12,34,640,360", nullptr);
    EXPECT_EQ(12, sink->windowX);
    EXPECT_EQ(34, sink->windowY);
    EXPECT_EQ(640, sink->windowWidth);
    EXPECT_EQ(360, sink->windowHeight);
    EXPECT_TRUE(sink->windowSet);
    EXPECT_TRUE(sink->windowChange);
    EXPECT_TRUE(sink->windowSizeOverride);
}

TEST_F(WesterosSinkTest, WindowAndDisplayPropertiesHandleInvalidAndRepeatedValues)
{
    g_object_set(sink, "window_set", "12,34", nullptr);
    EXPECT_FALSE(sink->windowSet);

    g_object_set(sink, "window_set", "0,0,1280,720", nullptr);
    EXPECT_TRUE(sink->windowSet);
    EXPECT_FALSE(sink->windowSizeOverride);
    sink->windowChange = FALSE;
    g_object_set(sink, "window_set", "0,0,1280,720", nullptr);
    EXPECT_FALSE(sink->windowChange);

    g_object_set(sink, "display-name", "wayland-test", nullptr);
    ASSERT_NE(nullptr, sink->displayName);
    EXPECT_STREQ("wayland-test", sink->displayName);
    g_object_set(sink, "display-name", nullptr, nullptr);
    EXPECT_EQ(nullptr, sink->displayName);
}

TEST_F(WesterosSinkTest, PublicPropertiesExposeVideoStateAndSetPresentationValues)
{
    g_object_set(sink, "opacity", 0.4f, "zorder", 0.75f, nullptr);
    EXPECT_FLOAT_EQ(0.4f, sink->opacity);
    EXPECT_FLOAT_EQ(0.75f, sink->zorder);

    sink->srcWidth = 1920;
    sink->srcHeight = 1080;
    sink->currentPTS = 90000;
    gint width = 0;
    gint height = 0;
    gint64 pts = 0;
    g_object_get(sink, "video_width", &width, "video_height", &height,
                 "video_pts", &pts, nullptr);
    EXPECT_EQ(1920, width);
    EXPECT_EQ(1080, height);
    EXPECT_EQ(90000, pts);
}

TEST_F(WesterosSinkTest, LinkChecksPeerCapsAndKeepsGstreamerPadLinkContract)
{
    GstPad *source = createSourcePad();
    GstPad *target = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, target);

    EXPECT_EQ(GST_PAD_LINK_OK, gst_pad_link(source, target));
    EXPECT_EQ(source, sink->peerPad);
    EXPECT_EQ(1, sink_backend_mock_state().acceptCapsCalls);

    EXPECT_TRUE(gst_pad_unlink(source, target));
    gst_object_unref(target);
    gst_object_unref(source);
}

TEST_F(WesterosSinkTest, LinkStartsBackendWhenDeferredStartWasRequested)
{
    sink->startAfterLink = TRUE;
    GstPad *source = createSourcePad();
    GstPad *target = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, target);

    EXPECT_EQ(GST_PAD_LINK_OK, gst_pad_link(source, target));
    EXPECT_FALSE(sink->startAfterLink);
    EXPECT_EQ(source, sink->peerPad);
    EXPECT_EQ(1, sink_backend_mock_state().startVideoCalls);
    gst_pad_unlink(source, target);
    EXPECT_EQ(nullptr, sink->peerPad);
    gst_object_unref(target);
    gst_object_unref(source);
}

TEST_F(WesterosSinkTest, UnsupportedBackendCapsAreObservedWithoutCorruptingPadState)
{
    sink_backend_mock_set_accept_caps(false);

    GstPad *source = createSourcePad();
    GstPad *target = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, target);
    EXPECT_EQ(GST_PAD_LINK_OK, gst_pad_link(source, target));
    EXPECT_EQ(1, sink_backend_mock_state().acceptCapsCalls);
    EXPECT_EQ(source, sink->peerPad);
    EXPECT_FALSE(sink_backend_mock_state().acceptCaps);
    gst_pad_unlink(source, target);
    gst_object_unref(target);
    gst_object_unref(source);
}

TEST_F(WesterosSinkTest, CapsEventUpdatesFrameRateAndInvokesBackendAcceptance)
{
    sink->passCaps = TRUE;
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    GstCaps *caps = gst_caps_from_string("video/x-raw,format=NV12,width=640,height=360,framerate=30000/1001");
    ASSERT_NE(nullptr, pad);
    ASSERT_NE(nullptr, caps);
    gst_pad_set_active(pad, TRUE);
    gst_pad_send_event(pad, gst_event_new_caps(caps));
    EXPECT_NEAR(30000.0 / 1001.0, sink->frameRate, 0.001);
    EXPECT_EQ(1, sink_backend_mock_state().acceptCapsCalls);
    gst_caps_unref(caps);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, CapsEventHandlesZeroRateAndPostsOversizeError)
{
    sink->passCaps = TRUE;
    sink->maxWidth = 320;
    sink->maxHeight = 240;
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    GstCaps *caps = gst_caps_from_string(
        "video/x-raw,format=NV12,width=640,height=360,framerate=0/1");
    ASSERT_NE(nullptr, pad);
    ASSERT_NE(nullptr, caps);
    gst_pad_set_active(pad, TRUE);
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_caps(caps)));
    EXPECT_DOUBLE_EQ(60.0, sink->frameRate);
    EXPECT_EQ(1, sink_backend_mock_state().acceptCapsCalls);
    gst_caps_unref(caps);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, FlushStartResetsEosAndRequestsBackendFlush)
{
    sink->eosEventSeen = TRUE;
    sink->needSegment = FALSE;
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    gst_pad_send_event(pad, gst_event_new_flush_start());
    EXPECT_TRUE(sink->flushStarted);
    EXPECT_TRUE(sink->needSegment);
    EXPECT_FALSE(sink->eosEventSeen);
    EXPECT_EQ(1, sink_backend_mock_state().flushCalls);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, FlushStopClearsFlushState)
{
    sink->flushStarted = TRUE;
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_flush_stop(TRUE)));
    EXPECT_FALSE(sink->flushStarted);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, SegmentEventSetsPlaybackPositionAndBackendStartPts)
{
    GstSegment segment;
    gst_segment_init(&segment, GST_FORMAT_TIME);
    segment.start = 2 * GST_SECOND;
    segment.position = 3 * GST_SECOND;
    segment.rate = 0.5;
    segment.applied_rate = 1.0;

    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    gst_pad_send_event(pad, gst_event_new_segment(&segment));
    EXPECT_FALSE(sink->needSegment);
    EXPECT_FLOAT_EQ(0.5f, sink->playbackRate);
    EXPECT_EQ(2 * GST_SECOND, sink->position);
    EXPECT_EQ(1, sink_backend_mock_state().setStartPtsCalls);
    EXPECT_EQ(180000, sink_backend_mock_state().lastStartPts);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, ElementQueriesReportLatencyAndCurrentPosition)
{
    sink->position = 7 * GST_SECOND;
    GstQuery *latency = gst_query_new_latency();
    ASSERT_TRUE(gst_element_query(GST_ELEMENT(sink), latency));
    gboolean live = TRUE;
    GstClockTime minimum = GST_CLOCK_TIME_NONE;
    GstClockTime maximum = GST_CLOCK_TIME_NONE;
    gst_query_parse_latency(latency, &live, &minimum, &maximum);
    EXPECT_FALSE(live);
    EXPECT_EQ(0, minimum);
    EXPECT_EQ(10 * GST_MSECOND, maximum);
    gst_query_unref(latency);

    GstQuery *position = gst_query_new_position(GST_FORMAT_TIME);
    ASSERT_TRUE(gst_element_query(GST_ELEMENT(sink), position));
    gint64 reported = -1;
    gst_query_parse_position(position, nullptr, &reported);
    EXPECT_EQ(7 * GST_SECOND, reported);
    gst_query_unref(position);
}

TEST_F(WesterosSinkTest, SinkPadQueryCanBeHandledByTheBackend)
{
    sink_backend_mock_set_handle_queries(true);
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    GstQuery *query = gst_query_new_custom(GST_QUERY_CUSTOM,
        gst_structure_new_empty("westeros-sink-l1-query"));
    ASSERT_NE(nullptr, query);
    EXPECT_TRUE(gst_pad_query(pad, query));
    EXPECT_EQ(1, sink_backend_mock_state().queryCalls);
    gst_query_unref(query);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, SinkPadQueryFallsBackWhenBackendDoesNotHandleIt)
{
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    GstQuery *query = gst_query_new_custom(GST_QUERY_CUSTOM,
        gst_structure_new_empty("westeros-sink-l1-fallback-query"));
    ASSERT_NE(nullptr, query);
    EXPECT_FALSE(gst_pad_query(pad, query));
    EXPECT_EQ(1, sink_backend_mock_state().queryCalls);
    gst_query_unref(query);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, PositionQueryUsesLinkedPeerWhenRequested)
{
    GstPad *source = createSourcePad();
    GstPad *target = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, target);
    gst_pad_set_query_function(source, GST_DEBUG_FUNCPTR(handlePeerPositionQuery));
    ASSERT_EQ(GST_PAD_LINK_OK, gst_pad_link(source, target));
    sink->queryPositionFromPeer = TRUE;

    GstQuery *query = gst_query_new_position(GST_FORMAT_TIME);
    ASSERT_TRUE(gst_element_query(GST_ELEMENT(sink), query));
    gint64 position = GST_CLOCK_TIME_NONE;
    gst_query_parse_position(query, nullptr, &position);
    EXPECT_EQ(11 * GST_SECOND, position);

    gst_query_unref(query);
    gst_pad_unlink(source, target);
    gst_object_unref(target);
    gst_object_unref(source);
}

TEST_F(WesterosSinkTest, DurationSeekingAndCustomQueriesAreForwardedToPeer)
{
    GstPad *source = createSourcePad();
    GstPad *target = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, source);
    ASSERT_NE(nullptr, target);
    gst_pad_set_query_function(source, GST_DEBUG_FUNCPTR(handlePeerPositionQuery));
    ASSERT_EQ(GST_PAD_LINK_OK, gst_pad_link(source, target));

    GstQuery *duration = gst_query_new_duration(GST_FORMAT_TIME);
    EXPECT_TRUE(gst_element_query(GST_ELEMENT(sink), duration));
    gint64 durationValue = -1;
    gst_query_parse_duration(duration, nullptr, &durationValue);
    EXPECT_EQ(42 * GST_SECOND, durationValue);
    gst_query_unref(duration);

    GstQuery *seeking = gst_query_new_seeking(GST_FORMAT_TIME);
    EXPECT_TRUE(gst_element_query(GST_ELEMENT(sink), seeking));
    gboolean seekable = FALSE;
    gst_query_parse_seeking(seeking, nullptr, &seekable, nullptr, nullptr);
    EXPECT_TRUE(seekable);
    gst_query_unref(seeking);

    GstQuery *custom = gst_query_new_custom(GST_QUERY_CUSTOM,
        gst_structure_new_empty("westeros-sink-l1-peer-query"));
    EXPECT_TRUE(gst_element_query(GST_ELEMENT(sink), custom));
    gst_query_unref(custom);

    gst_pad_unlink(source, target);
    gst_object_unref(target);
    gst_object_unref(source);
}

TEST_F(WesterosSinkTest, TimeCodeMetadataIsQueuedUpdatedAndPresented)
{
    g_object_set(sink, "enable-timecode", TRUE, nullptr);
    GstBaseSinkClass *klass = GST_BASE_SINK_GET_CLASS(sink);
    for (guint seconds = 1; seconds <= 31; ++seconds) {
        GstBuffer *buffer = gst_buffer_new_allocate(nullptr, 16, nullptr);
        ASSERT_NE(nullptr, buffer);
        GstVideoTimeCode timeCode;
        gst_video_time_code_init(&timeCode, 30, 1, nullptr,
            GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, seconds, 0, 0);
        ASSERT_NE(nullptr, gst_buffer_add_video_time_code_meta(buffer, &timeCode));
        GST_BUFFER_PTS(buffer) = seconds * GST_SECOND;
        EXPECT_EQ(GST_FLOW_OK, klass->render(GST_BASE_SINK(sink), buffer));
        gst_buffer_unref(buffer);
    }
    ASSERT_EQ(31, sink->timeCodeCount);
    EXPECT_EQ(60, sink->timeCodeCapacity);

    GstBuffer *duplicate = gst_buffer_new_allocate(nullptr, 16, nullptr);
    ASSERT_NE(nullptr, duplicate);
    GstVideoTimeCode duplicateTimeCode;
    gst_video_time_code_init(&duplicateTimeCode, 30, 1, nullptr,
        GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 10, 0, 0);
    ASSERT_NE(nullptr, gst_buffer_add_video_time_code_meta(duplicate, &duplicateTimeCode));
    GST_BUFFER_PTS(duplicate) = 9 * GST_SECOND + GST_SECOND / 2;
    EXPECT_EQ(GST_FLOW_OK, klass->render(GST_BASE_SINK(sink), duplicate));
    EXPECT_EQ(855000, sink->timeCodes[9].position);

    static guint signalId = 0;
    if (signalId == 0) {
        signalId = g_signal_new("l1-time-code", G_TYPE_FROM_INSTANCE(sink),
            G_SIGNAL_RUN_LAST, 0, nullptr, nullptr, nullptr, G_TYPE_NONE, 3,
            G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
    }
    TimeCodeSignalState signalState;
    g_signal_connect(sink, "l1-time-code", G_CALLBACK(recordTimeCodeSignal),
                     &signalState);
    sink->currentPTS = 855000;
    LOCK(sink);
    sink->timeCodePresent(sink, sink->currentPTS, signalId);
    UNLOCK(sink);
    EXPECT_EQ(1, signalState.calls);
    EXPECT_EQ(0u, signalState.hours);
    EXPECT_EQ(0u, signalState.minutes);
    EXPECT_EQ(10u, signalState.seconds);
    EXPECT_EQ(21, sink->timeCodeCount);

    GstBuffer *outOfOrder = gst_buffer_new_allocate(nullptr, 16, nullptr);
    ASSERT_NE(nullptr, outOfOrder);
    GstVideoTimeCode earlierTimeCode;
    gst_video_time_code_init(&earlierTimeCode, 30, 1, nullptr,
        GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 9, 0, 0);
    ASSERT_NE(nullptr, gst_buffer_add_video_time_code_meta(outOfOrder, &earlierTimeCode));
    GST_BUFFER_PTS(outOfOrder) = 9 * GST_SECOND;
    EXPECT_EQ(GST_FLOW_OK, klass->render(GST_BASE_SINK(sink), outOfOrder));
    EXPECT_EQ(21, sink->timeCodeCount);

    g_object_set(sink, "enable-timecode", FALSE, nullptr);
    EXPECT_EQ(nullptr, sink->timeCodes);
    gst_buffer_unref(outOfOrder);
    gst_buffer_unref(duplicate);
}

TEST_F(WesterosSinkTest, SegmentWithDefaultAppliedRateIsIgnoredAfterNonDefaultRate)
{
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);

    GstSegment first;
    gst_segment_init(&first, GST_FORMAT_TIME);
    first.start = GST_SECOND;
    first.position = GST_SECOND;
    first.applied_rate = 2.0;
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_segment(&first)));
    const gint64 originalStartPts = sink->startPTS;

    GstSegment redundant;
    gst_segment_init(&redundant, GST_FORMAT_TIME);
    redundant.start = 5 * GST_SECOND;
    redundant.position = 5 * GST_SECOND;
    redundant.applied_rate = 1.0;
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_segment(&redundant)));
    EXPECT_EQ(originalStartPts, sink->startPTS);

    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
}

TEST_F(WesterosSinkTest, EosAlreadyDetectedDoesNotNotifyBackendAgain)
{
    ASSERT_NE(GST_STATE_CHANGE_FAILURE,
              gst_element_set_state(GST_ELEMENT(sink), GST_STATE_PLAYING));
    gst_westeros_sink_eos_detected(sink);
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    gst_pad_send_event(pad, gst_event_new_eos());
    EXPECT_TRUE(sink->eosEventSeen);
    EXPECT_EQ(0, sink_backend_mock_state().eosCalls);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
    gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL);
}

TEST_F(WesterosSinkTest, BaseSinkLifecycleHooksSucceed)
{
    GstBaseSinkClass *klass = GST_BASE_SINK_GET_CLASS(sink);
    ASSERT_NE(nullptr, klass->start);
    ASSERT_NE(nullptr, klass->stop);
    ASSERT_NE(nullptr, klass->unlock);
    ASSERT_NE(nullptr, klass->unlock_stop);
    EXPECT_TRUE(klass->start(GST_BASE_SINK(sink)));
    EXPECT_TRUE(klass->unlock(GST_BASE_SINK(sink)));
    EXPECT_TRUE(klass->unlock_stop(GST_BASE_SINK(sink)));
    EXPECT_TRUE(klass->stop(GST_BASE_SINK(sink)));
}

TEST_F(WesterosSinkTest, PadAndElementEventHooksCanConsumeEvents)
{
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    sink->processPadEvent = interceptPadEvent;
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_flush_start()));
    EXPECT_FALSE(sink->flushStarted);
    sink->processPadEvent = nullptr;
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);

    sendEventState = {};
    sink->processSendEvent = interceptElementEvent;
    GstEvent *event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM,
        gst_structure_new_empty("westeros-sink-l1-consumed-event"));
    ASSERT_NE(nullptr, event);
    EXPECT_TRUE(GST_ELEMENT_GET_CLASS(sink)->send_event(GST_ELEMENT(sink), event));
    EXPECT_EQ(1, sendEventState.calls);

    sendEventState.result = FALSE;
    event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM,
        gst_structure_new_empty("westeros-sink-l1-rejected-event"));
    ASSERT_NE(nullptr, event);
    EXPECT_FALSE(GST_ELEMENT_GET_CLASS(sink)->send_event(GST_ELEMENT(sink), event));
    EXPECT_EQ(2, sendEventState.calls);
}

TEST_F(WesterosSinkTest, RenderDelegatesBufferAndPrerollHonorsFlushRejection)
{
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, 16, nullptr);
    ASSERT_NE(nullptr, buffer);
    GstBaseSinkClass *klass = GST_BASE_SINK_GET_CLASS(sink);
    ASSERT_NE(nullptr, klass->render);
    EXPECT_EQ(GST_FLOW_OK, klass->render(GST_BASE_SINK(sink), buffer));
    EXPECT_EQ(1, sink_backend_mock_state().renderCalls);
    EXPECT_FALSE(sink->eosDetected);

    sink->rejectPrerollBuffers = TRUE;
    EXPECT_EQ(GST_FLOW_FLUSHING, klass->preroll(GST_BASE_SINK(sink), buffer));
    sink->rejectPrerollBuffers = FALSE;
    EXPECT_EQ(GST_FLOW_OK, klass->preroll(GST_BASE_SINK(sink), buffer));
    gst_buffer_unref(buffer);
}

TEST_F(WesterosSinkTest, EosDetectionRecordsBackendCompletion)
{
    gst_westeros_sink_eos_detected(sink);
    EXPECT_TRUE(sink->eosDetected);
    EXPECT_FALSE(sink->eosEventSeen);
}

TEST_F(WesterosSinkTest, PlayingEosEventIsHandedToTheBackend)
{
    ASSERT_NE(GST_STATE_CHANGE_FAILURE,
              gst_element_set_state(GST_ELEMENT(sink), GST_STATE_PLAYING));
    GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(sink), "sink");
    ASSERT_NE(nullptr, pad);
    gst_pad_set_active(pad, TRUE);
    EXPECT_TRUE(gst_pad_send_event(pad, gst_event_new_eos()));
    EXPECT_TRUE(sink->eosEventSeen);
    EXPECT_EQ(1, sink_backend_mock_state().eosCalls);
    gst_pad_set_active(pad, FALSE);
    gst_object_unref(pad);
    gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL);
}

TEST_F(WesterosSinkTest, StateTransitionsAreDelegatedToBackend)
{
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(GST_ELEMENT(sink), GST_STATE_READY));
    EXPECT_EQ(GST_STATE_CHANGE_SUCCESS, gst_element_set_state(GST_ELEMENT(sink), GST_STATE_NULL));
    EXPECT_GE(sink_backend_mock_state().stateTransitionCalls, 2);
}

} // namespace
