# Rialto-GStreamer Integration Guide for Playback Metrics (V1.1-RG-1 & V1.1-RG-2)

## Overview
With Rialto Phase 1 complete, rialto-gstreamer can now fully implement metrics support aligned with Proposal V1.1 §4. This guide covers:
- **V1.1-RG-1**: Metric subscription and event notification
- **V1.1-RG-2**: Frame counter query from sink

## What's Now Available in Rialto

### New in Rialto API
1. **getFrameCounters()** - Query current frame counters
   - Location: IMediaPipeline interface
   - Signature: `bool getFrameCounters(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames, uint64_t &corruptedFrames, uint64_t &timestamp)`
   - Returns: rendered, dropped, corrupted frame counts + server timestamp

2. **notifyMetric()** - Receive metric events (already existed, now fully integrated)
   - Location: IMediaPipelineClient interface
   - Signature: `void notifyMetric(const MetricEvent &metric)`
   - Called by Rialto server when metrics are generated

3. **setMetricSubscription()** - Filter metrics (already existed)
   - Location: IMediaPipeline interface
   - Signature: `bool setMetricSubscription(const std::vector<MetricType> &types)`
   - Allows selective receiving of metric types

## Data Structures Now Available

### MetricEvent (from RialtoMetrics)
```cpp
struct MetricEvent
{
    MetricType type;                       // VIDEO_FRAME_DROP, AUDIO_FRAME_DROP, etc. (1-7)
    MetricPhase phase;                     // OCCURRENCE, EPISODE_STARTED, EPISODE_RESOLVED
    int32_t sourceId;                      // Source this metric belongs to
    std::optional<int64_t> presentationTimeStampNs;    // PTS if known
    std::optional<uint64_t> count;         // Count associated with event
    std::optional<uint64_t> durationNs;    // Duration if applicable
    uint64_t observedAtMonotonicNs;        // Monotonic time of observation
    int64_t observedAtUnixNs;              // Unix time of observation
};
```

### VideoFrameCounters (from MediaCommon)
```cpp
struct VideoFrameCounters
{
    uint32_t size;                         // sizeof(VideoFrameCounters)
    uint64_t renderedFrames;               // Frames successfully rendered
    uint64_t droppedFrames;                // Frames dropped (presentation failures)
    uint64_t corruptedFrames;              // Frames with decode errors
};
```

## Integration Points

### 1. Subscribing to Metrics (V1.1-RG-1)

#### In GStreamerMSEMediaPlayerClient
```cpp
// Subscribe to specific metrics
std::vector<firebolt::rialto::MetricType> subscribedTypes = {
    firebolt::rialto::MetricType::VIDEO_FRAME_DROP,
    firebolt::rialto::MetricType::AUDIO_FRAME_DROP,
    firebolt::rialto::MetricType::VIDEO_DECODE_ERROR
};

bool success = m_clientBackend->setMetricSubscription(subscribedTypes);
if (!success)
{
    GST_WARNING("Failed to subscribe to metrics");
}
```

#### In PullModeVideoPlaybackDelegate or equivalent
```cpp
// Subscribe when pipeline is created
if (m_mediaPipeline)
{
    std::vector<firebolt::rialto::MetricType> allMetrics = {
        firebolt::rialto::MetricType::VIDEO_FRAME_DROP,
        firebolt::rialto::MetricType::AUDIO_FRAME_DROP,
        firebolt::rialto::MetricType::VIDEO_DECODE_ERROR,
        firebolt::rialto::MetricType::AUDIO_DECODE_ERROR,
        firebolt::rialto::MetricType::VIDEO_FRAME_REPEAT,
        firebolt::rialto::MetricType::AUDIO_GAP,
        firebolt::rialto::MetricType::BUFFER_UNDERFLOW
    };
    m_mediaPipeline->setMetricSubscription(allMetrics);
}
```

### 2. Receiving Metric Events (V1.1-RG-1)

#### Already Implemented
The infrastructure is already in place in GStreamerMSEMediaPlayerClient:

```cpp
class GStreamerMSEMediaPlayerClient : public IMediaPipelineClient
{
    // Already implemented:
    void notifyMetric(const firebolt::rialto::MetricEvent &metric) override;
    void onMetricEvent(int32_t sourceId, const firebolt::rialto::MetricEvent &event);
    void postMetricEventOnBus(int32_t sourceId, const firebolt::rialto::MetricEvent &event);
};
```

#### Event Posting to GStreamer Bus
Events are posted as GStreamer messages:
```
media-pipeline-metric structure:
  metric: G_TYPE_UINT (MetricType value 1-7)
  observation-kind: G_TYPE_UINT (MetricPhase value 1-3)
  source_id: G_TYPE_UINT
  pts-ns: G_TYPE_INT64 (optional)
  count: G_TYPE_UINT64 (optional)
  duration-ns: G_TYPE_UINT64 (optional)
  observed-at-monotonic-ns: G_TYPE_UINT64
  observed-at-unix-ns: G_TYPE_INT64
```

Sink elements can subscribe to these bus messages:
```cpp
GstBus *bus = gst_element_get_bus(pipeline);
gst_bus_add_watch(bus, bus_message_callback, userdata);

// In callback:
if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_APPLICATION)
{
    const GstStructure *structure = gst_message_get_structure(msg);
    if (gst_structure_has_name(structure, "media-pipeline-metric"))
    {
        // Handle metric event
        guint metric_type = 0;
        gst_structure_get_uint(structure, "metric", &metric_type);
        // ... process metric
    }
}
```

### 3. Querying Frame Counters (V1.1-RG-2)

#### In RialtoGStreamerMSEVideoSink or equivalent
```cpp
#include <IMediaPipeline.h>

// Query current frame counters from Rialto server
uint64_t renderedFrames, droppedFrames, corruptedFrames, timestamp;

if (m_mediaPipeline->getFrameCounters(m_sourceId, renderedFrames, droppedFrames, 
                                      corruptedFrames, timestamp))
{
    GST_DEBUG("Frame counters - Rendered: %" G_GUINT64_FORMAT 
              ", Dropped: %" G_GUINT64_FORMAT 
              ", Corrupted: %" G_GUINT64_FORMAT 
              ", Timestamp: %" G_GUINT64_FORMAT,
              renderedFrames, droppedFrames, corruptedFrames, timestamp);
    
    // Use counters for quality monitoring, ABR decisions, etc.
    updatePlaybackQualityMetrics(renderedFrames, droppedFrames, corruptedFrames);
}
else
{
    GST_WARNING("Failed to retrieve frame counters for sourceId: %d", m_sourceId);
}
```

#### Periodic Monitoring
```cpp
// In a periodic timer callback (e.g., every 1 second)
static gboolean metrics_monitor_callback(gpointer user_data)
{
    GStreamerMSEVideoSink *sink = (GStreamerMSEVideoSink *)user_data;
    
    uint64_t rendered, dropped, corrupted, timestamp;
    if (sink->m_mediaPipeline->getFrameCounters(sink->m_sourceId, rendered, dropped,
                                                 corrupted, timestamp))
    {
        // Log or update UI
        sink->onMetricsSnapshot({rendered, dropped, corrupted, timestamp});
    }
    
    return G_SOURCE_CONTINUE; // Keep timer running
}

// In sink initialization:
// g_timeout_add_seconds(1, metrics_monitor_callback, sink);
```

## Metric Types and Semantics

### MetricType Enum Values
```cpp
enum class MetricType : uint32_t
{
    VIDEO_FRAME_DROP = 1,       // Video frame dropped by sink
    AUDIO_FRAME_DROP = 2,       // Audio frame dropped by sink
    VIDEO_DECODE_ERROR = 3,     // Video decode failed
    AUDIO_DECODE_ERROR = 4,     // Audio decode failed
    VIDEO_FRAME_REPEAT = 5,     // Video frame repeated for presentation
    AUDIO_GAP = 6,              // Gap in audio samples
    BUFFER_UNDERFLOW = 7        // Buffer underflow occurred
};
```

### MetricPhase Enum Values
```cpp
enum class MetricPhase : uint32_t
{
    OCCURRENCE = 1,             // Single metric occurrence
    EPISODE_STARTED = 2,        // Start of an episode/sequence
    EPISODE_RESOLVED = 3        // End of episode/sequence
};
```

## Implementation Patterns

### Pattern 1: Simple Counter Query
```cpp
class MyMetricsCollector
{
public:
    void queryAndReport()
    {
        uint64_t rendered, dropped, corrupted, timestamp;
        
        if (m_pipeline->getFrameCounters(m_videoSourceId, rendered, dropped, 
                                         corrupted, timestamp))
        {
            float dropRate = (dropped > 0) ? 
                (100.0f * dropped) / (rendered + dropped) : 0.0f;
            
            GST_INFO("Video Quality: %.1f%% drop rate, %llu corrupted frames",
                     dropRate, corrupted);
        }
    }
    
private:
    std::shared_ptr<IMediaPipeline> m_pipeline;
    int32_t m_videoSourceId;
};
```

### Pattern 2: Event-Driven Monitoring
```cpp
class MyMediaPlayerClient : public IMediaPipelineClient
{
    void notifyMetric(const MetricEvent &metric) override
    {
        switch (metric.type)
        {
        case MetricType::VIDEO_FRAME_DROP:
            GST_WARNING("Video frame drop detected at PTS %lld", 
                       metric.presentationTimeStampNs.value_or(0));
            break;
            
        case MetricType::BUFFER_UNDERFLOW:
            GST_ERROR("Buffer underflow!");
            triggerAdaptiveBitrateAdjustment();
            break;
            
        default:
            break;
        }
    }
    
private:
    void triggerAdaptiveBitrateAdjustment() { /* ... */ }
};
```

### Pattern 3: Subscription Filtering
```cpp
// Only monitor frame drops and errors, ignore other metrics
std::vector<MetricType> importantMetrics = {
    MetricType::VIDEO_FRAME_DROP,
    MetricType::VIDEO_DECODE_ERROR,
    MetricType::BUFFER_UNDERFLOW
};

m_pipeline->setMetricSubscription(importantMetrics);

// Later, clear subscription to stop receiving metrics
m_pipeline->setMetricSubscription(std::vector<MetricType>());  // Empty = no metrics
```

## Migration from getStats()

### Old Approach
```cpp
// getStats() provides only rendered and dropped frames
uint64_t rendered, dropped;
if (m_pipeline->getStats(sourceId, rendered, dropped))
{
    // Can't access corrupted frame count
}
```

### New Approach
```cpp
// getFrameCounters() provides complete frame statistics
uint64_t rendered, dropped, corrupted, timestamp;
if (m_pipeline->getFrameCounters(sourceId, rendered, dropped, corrupted, timestamp))
{
    // Access all frame counters including corrupted frames (decode errors)
    // timestamp indicates when server took the snapshot
}
```

## Compatibility Notes

### Backward Compatibility
- `getStats()` continues to work (provided for compatibility)
- `getFrameCounters()` is the recommended new method
- Both can be used during transition period

### Server Version Compatibility
- Rialto Phase 1 implementation provides `getFrameCounters()`
- Older servers without this method will return false
- Client should handle gracefully:
```cpp
if (!pipeline->getFrameCounters(sourceId, rendered, dropped, corrupted, timestamp))
{
    // Fall back to older getStats() if available
    if (!pipeline->getStats(sourceId, rendered, dropped))
    {
        GST_WARNING("Neither getFrameCounters nor getStats available");
    }
}
```

## Testing Checklist

### Unit Tests
- [ ] Protobuf message serialization for MetricEvent
- [ ] getFrameCounters() return value parsing
- [ ] Counter value monotonicity (never decrease)
- [ ] Metric subscription filtering logic

### Integration Tests
- [ ] End-to-end: Server metrics → RialtoMetrics cache → IPC → Client
- [ ] Multiple sources tracked independently
- [ ] Metric events reach GStreamer bus correctly
- [ ] Frame counter updates reflect sink state changes

### Manual Testing
- [ ] Play video and verify getFrameCounters() returns values
- [ ] Subscribe to metrics and verify events appear on GStreamer bus
- [ ] Simulate frame drops and verify counter increments
- [ ] Test with multiple audio/video sources simultaneously

## Performance Considerations

### getFrameCounters() Call
- **Latency**: ~1-10ms (cached values, no blocking I/O)
- **Frequency**: Can be called up to 60 Hz if needed (e.g., per video frame)
- **CPU Impact**: Minimal - simple cache lookup

### Metric Events
- **Latency**: <100ms from event generation to client notification
- **Frequency**: Depends on playback quality (potentially many per second if drops occur)
- **CPU Impact**: Low - event dispatch only when metrics exist

### Recommendations
- Query counters at most every 100ms (10 Hz) for monitoring
- Use subscription filtering to reduce event volume
- Process metric events asynchronously if filtering is complex

## Debugging Tips

### Enable Detailed Logging
```cpp
// In GStreamerMSEMediaPlayerClient
GST_DEBUG_CATEGORY_INIT(gstreamer_mse_media_player_client, 
                       "rialto-client-metrics", 0, 
                       "Rialto Metrics Client");

// Set debug level:
// export GST_DEBUG=rialto-client-metrics:DEBUG
```

### Verify Metric Reception
```cpp
void onMetricEvent(const MetricEvent &event)
{
    GST_INFO("Metric: type=%u, phase=%u, sourceId=%d, "
             "pts=%s, count=%s, duration=%s, "
             "observedAtMonotonicNs=%" G_GUINT64_FORMAT
             ", observedAtUnixNs=%" G_GINT64_FORMAT,
             static_cast<uint32_t>(event.type),
             static_cast<uint32_t>(event.phase),
             event.sourceId,
             event.presentationTimeStampNs.has_value() ? 
                std::to_string(event.presentationTimeStampNs.value()).c_str() : "absent",
             event.count.has_value() ? 
                std::to_string(event.count.value()).c_str() : "absent",
             event.durationNs.has_value() ? 
                std::to_string(event.durationNs.value()).c_str() : "absent",
             event.observedAtMonotonicNs,
             event.observedAtUnixNs);
}
```

### Verify Frame Counter Cache
```cpp
// Check if RialtoMetrics has data for this source
uint64_t rendered, dropped, corrupted, timestamp;
bool hasData = pipeline->getFrameCounters(sourceId, rendered, dropped, 
                                          corrupted, timestamp);
if (hasData)
{
    GST_INFO("Cache hit: rendered=%llu, dropped=%llu, corrupted=%llu, ts=%llu",
             rendered, dropped, corrupted, timestamp);
}
else
{
    GST_WARNING("Cache miss or error for sourceId=%d", sourceId);
}
```

## References

- **Proposal V1.1 §4**: Media Metrics specification
- **IMediaPipeline.h**: Public API interface
- **IMediaPipelineClient.h**: Client callback interface
- **MediaCommon.h**: Shared data structures
- **MediaPipelineIpc.cpp**: Client-side IPC implementation
- **MediaPipelineModuleService.cpp**: Server-side RPC handler

## Summary

Rialto Phase 1 provides rialto-gstreamer with full access to playback metrics through three methods:

1. **getFrameCounters()** - Synchronous query for frame counts
2. **notifyMetric()** - Asynchronous event notifications
3. **setMetricSubscription()** - Selective metric filtering

These methods enable V1.1-RG-1 (metric subscription) and V1.1-RG-2 (frame counter query) implementations with full integration into the GStreamer pipeline.
