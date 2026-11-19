# Rialto V1.1 - Phase 2 to Phase 8 Detailed Guide

**Last Updated**: 2026-11-15T21:17:20+05:30

---

## Phase Overview Matrix

| Phase | Section | Feature | Effort | Complexity | Dependencies | Optional |
|-------|---------|---------|--------|-----------|--------------|----------|
| ✅ **1** | §4 | Media Metrics | 1 day | Low | None | No |
| **2** | §4 | Metric Optimization | 2.5 days | Medium | Phase 1 | No |
| **3** | §3 | Video Frame Capture | 7-10 days | **VERY HIGH** | SoC HAL | No |
| **4** | §5 | Audio Discontinuity | 3-5 days | Medium | None | No |
| **5** | §6 | KeyStatus Parity | 1-2 days | Low | None | No |
| **6** | §7 | Individualization | 2-3 days | Medium | None | **YES** |
| **7** | §9 | Persistent Licenses | 4-6 days | High | None | No |
| **8** | §10 | App Lifecycle Reclaim | 5-7 days | High | Phase 7 | No |

**Total Remaining**: 25-40 days for full V1.1

---

## Phase 2: Metric Optimization (§4 Phase 2)

**Timeline**: 2.5 days  
**Complexity**: Medium  
**Effort**: Medium  
**Priority**: Medium (Recommended for production)

### What Changes

#### 2.1 Per-Client Subscription Filtering
**Current State**: Single global subscription list  
**Target State**: Per-client subscription management

```cpp
// BEFORE (Phase 1)
std::unordered_set<uint32_t> m_subscribedMetricTypes;

// AFTER (Phase 2)
struct ClientMetricSubscription {
    uint32_t clientId;
    std::unordered_set<uint32_t> metricTypes;
    bool isActive;
    timestamp createdAt;
};
std::unordered_map<uint32_t, ClientMetricSubscription> m_clientSubscriptions;
```

**Changes Required**:
- Track per-client subscriptions
- Support multiple concurrent clients
- Maintain subscription state per client
- Add client lifecycle management

**Files Affected**:
- `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp`
- `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.h`

#### 2.2 Event Batching

**Current Flow**: 
```
notifyMetric() → postMetricEventOnBus() [one-by-one]
```

**Optimized Flow**:
```
notifyMetric() → [batch queue] → 
  [time-based or count-based trigger] → 
  postBatchedMetricsOnBus()
```

**Configuration**:
- Batch size: 10-50 events
- Batch timeout: 10-100ms
- Adaptive batching based on load

**Implementation**:
- Add metric queue
- Implement batch timer
- Add batch post logic
- Maintain ordering guarantees

**Files Affected**:
- `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.cpp`
- `rialto-gstreamer/source/GStreamerMSEMediaPlayerClient.h`

#### 2.3 Performance Metrics

**Measure**:
- Metric notification latency (target: <5ms p99)
- RPC overhead reduction (target: 50-70%)
- Memory footprint (target: <100KB for queues)
- CPU impact of batching

**Success Criteria**:
- Per-client subscription working correctly
- No metric loss during batching
- Latency maintained under target
- No deadlocks or race conditions

### Testing Requirements

| Test | Scope |
|------|-------|
| Multiple clients with different subscriptions | Integration |
| Batch boundary conditions (full, partial, timeout) | Unit |
| Metric ordering in batches | Unit |
| Performance under high frequency | Load |
| Memory growth over time | Stability |
| Client subscription lifecycle | Integration |

### Use Cases

1. **High-frequency metrics** (1000+ events/sec)
2. **Multiple concurrent clients** with different filters
3. **Resource-constrained environments** needing efficiency
4. **QoS scenarios** requiring deterministic latency

---

## Phase 3: Video Frame Capture (§3)

**Timeline**: 7-10 days  
**Complexity**: ⚠️ **VERY HIGH**  
**Effort**: Very High  
**Priority**: Medium (if Netflix VaaT V2 needed)  
**Dependencies**: SoC HAL, GPU drivers, DMA-BUF support

### ⚠️ WARNING - HIGHEST COMPLEXITY PHASE

This phase requires:
- Deep understanding of GPU APIs (GL, Vulkan)
- DMA-BUF buffer management
- SoC HAL integration
- Color space conversions
- Frame pool lifecycle management
- Memory mapping and synchronization

### What Changes

#### 3.1 New IVideoFrameCapture Interface

```cpp
class IVideoFrameCapture {
public:
    virtual ~IVideoFrameCapture() = default;
    
    // Capabilities query
    virtual bool getCapabilities(VideoFrameCaptureCaps &caps) = 0;
    
    // Pool management
    virtual bool registerCapturePool(GraphicsAPIType api, const std::string &poolId) = 0;
    virtual bool unregisterCapturePool(const std::string &poolId) = 0;
    
    // Frame retrieval
    virtual bool getFrameCapture(VideoFrame &frame) = 0;
    virtual bool returnFrame(const VideoFrame &frame) = 0;
};
```

#### 3.2 New Data Structures

```cpp
enum class GraphicsAPIType {
    OPENGL,
    VULKAN
};

enum class GraphicsTextureLayout {
    RGBA,
    RGBA_SEPARATE_ALPHA,
    NV12,
    I420
};

struct VideoFrameCaptureCaps {
    GraphicsAPIType supportedApi;
    std::vector<GraphicsTextureLayout> supportedLayouts;
    uint32_t maxBuffers;
    bool supportsZeroCopy;
    ColorMetadata colorMetadata;
};

struct VideoFrame {
    uint64_t frameId;
    int64_t presentationTimeStampNs;
    GraphicsTextureLayout layout;
    
    // GPU handles
    std::vector<uint32_t> glTextureHandles;  // For OpenGL
    std::vector<VkImage> vulkanImages;       // For Vulkan
    
    // DMA-BUF (if applicable)
    std::vector<int> dmaFds;
    std::vector<uint32_t> pitches;
    std::vector<uint32_t> offsets;
    
    ColorMetadata colorMetadata;
    uint32_t poolId;
};
```

#### 3.3 Pipeline Integration

**Changes to IMediaPipeline**:
```cpp
virtual IVideoFrameCapture *getVideoFrameCapture() = 0;
virtual bool setVideoCapture(bool enable) = 0;
```

**Changes to IMediaPipelineClient**:
```cpp
virtual void onVideoCapture(const VideoFrame &frame) = 0;
```

#### 3.4 SoC HAL Integration

**New Contract**:
- Query GPU capture capabilities
- Register capture pool with HAL
- Get captured frames from HAL
- Return frames for reuse
- Handle color metadata

**Files Affected**:
- `rialto/media/public/include/IVideoFrameCapture.h` (NEW)
- `rialto/media/public/include/IMediaPipeline.h` (UPDATE)
- `rialto/media/public/include/IMediaPipelineClient.h` (UPDATE)
- `rialto/media/public/include/MediaCommon.h` (NEW structures)
- `rialto/proto/videoframecapturemodule.proto` (NEW)
- `rialto/media/server/` (Implement capture logic)
- `rialto-gstreamer/source/` (Implement consumer)

### Challenges

1. **GPU memory management** - Pool lifecycle, synchronization
2. **Multiple graphics APIs** - GL, Vulkan, potentially others
3. **DMA-BUF coordination** - Descriptor passing, lifetime
4. **Color space handling** - YUV/RGB conversion, metadata
5. **Frame ordering** - Timestamps, sequence numbers
6. **Zero-copy efficiency** - Avoiding copies while maintaining safety

### Testing Requirements

| Test Type | Coverage |
|-----------|----------|
| Unit | API contracts, data structures, enums |
| Integration | GL/Vulkan capture pipelines |
| Performance | Zero-copy latency, throughput |
| Compatibility | Different SoC implementations |
| Stress | Pool exhaustion, rapid acquire/release |

### Use Cases

1. **Netflix VaaT V2** - Texture-based rendering
2. **Texture consumers** - Direct GPU rendering without copies
3. **PGIO** - Post-processing GPU operations
4. **Recording pipelines** - High-performance frame capture

---

## Phase 4: Audio Discontinuity Metadata (§5)

**Timeline**: 3-5 days  
**Complexity**: Medium  
**Effort**: Medium  
**Priority**: Medium  
**Dependencies**: None (Phase 3 independent)

### What Changes

#### 4.1 AudioDiscontinuity Structure

```cpp
enum class AudioDiscontinuityType {
    CONTENT_TRANSITION,      // Program boundary
    PRESENTATION_GAP,        // Missing frames
    DECODE_ERROR_RECOVERY,   // After decode error
    AD_INSERTION,            // Ad break
    OTHER                    // Unspecified
};

struct AudioDiscontinuity {
    AudioDiscontinuityType type;
    int64_t discontinuityDurationMs;
    int32_t sourceId;
    int64_t presentationTimestampNs;
};
```

#### 4.2 Integration with MediaSegmentAudio

```cpp
// BEFORE
struct MediaSegmentAudio {
    int32_t sourceId;
    int64_t presentationTimeStampNs;
    std::shared_ptr<const MediaBuffer> data;
};

// AFTER
struct MediaSegmentAudio {
    int32_t sourceId;
    int64_t presentationTimeStampNs;
    std::shared_ptr<const MediaBuffer> data;
    
    // NEW
    std::optional<AudioDiscontinuity> discontinuity;
};
```

#### 4.3 Deprecation of processAudioGap()

```cpp
// DEPRECATED (kept for compatibility)
bool processAudioGap(int32_t sourceId, int64_t audioDurationMs);

// NEW RECOMMENDED PATH
// Use AudioDiscontinuity in MediaSegmentAudio instead
```

#### 4.4 Server-Side Changes

**In IMediaPipeline**:
```cpp
virtual bool attachAudio(int32_t sourceId) = 0;
// Now handles AudioDiscontinuity metadata in segments
```

**In Media Pipeline Server**:
- Attach discontinuity metadata to audio segments
- Convert sideband `processAudioGap()` to metadata
- Timestamp alignment

#### 4.5 GStreamer Integration

**Client-side**:
- Extract `AudioDiscontinuity` from segments
- Post to GStreamer bus (new message type)
- Route to audio elements

**Protocol**:
```protobuf
message AudioDiscontinuityMessage {
    enum Type {
        CONTENT_TRANSITION = 0;
        PRESENTATION_GAP = 1;
        DECODE_ERROR_RECOVERY = 2;
        AD_INSERTION = 3;
        OTHER = 4;
    }
    Type type = 1;
    int64 discontinuity_duration_ms = 2;
    int32 source_id = 3;
    int64 presentation_timestamp_ns = 4;
}
```

**Files Affected**:
- `rialto/media/public/include/IMediaPipeline.h` (ADD AudioDiscontinuity)
- `rialto/media/public/include/MediaCommon.h` (NEW AudioDiscontinuity)
- `rialto/proto/metadata.proto` (ADD AudioDiscontinuity)
- `rialto/proto/mediapipelinemodule.proto` (UPDATE segment)
- `rialto-gstreamer/source/` (Handle discontinuity)

### Testing Requirements

| Test | Coverage |
|------|----------|
| Segment attachment | Metadata properly carried |
| Timestamp alignment | Correct audio sample attachment |
| Deprecated path | processAudioGap() still works |
| GStreamer routing | Message posted correctly |
| Multiple discontinuities | Sequence handling |

### Use Cases

1. **Program transitions** - Content boundary markers
2. **Ad breaks** - Ad insertion tracking
3. **Live content** - Presentation gaps
4. **Audio sync recovery** - Post-decode-error resumption

---

## Phase 5: KeyStatus Parity (§6)

**Timeline**: 1-2 days  
**Complexity**: Low ⚡  
**Effort**: Low  
**Priority**: High (Quick win, compliance)

### What Changes

#### 5.1 New KeyStatus Enum Values

```cpp
enum class KeyStatus {
    USABLE,
    EXPIRED,
    RELEASED,
    RESTRICTED,
    PENDING,
    ERROR,
    INTERNAL_ERROR,
    
    // NEW (Phase 5)
    OUTPUT_DOWNSCALED,    // Output resolution reduced (HDCP)
    USABLE_IN_FUTURE      // Key will be usable later
};
```

#### 5.2 OCDM Conversion Bug Fix

**Issue**: KeyStatus mapping between OCDM and Rialto misses new enum values

**Fix**:
```cpp
// BEFORE
KeyStatus ocdmToRialto(OCDM_KeyStatus ocdmStatus) {
    switch (ocdmStatus) {
        case OCDM_USABLE: return KeyStatus::USABLE;
        case OCDM_EXPIRED: return KeyStatus::EXPIRED;
        // ... missing OUTPUT_DOWNSCALED, USABLE_IN_FUTURE
    }
}

// AFTER
KeyStatus ocdmToRialto(OCDM_KeyStatus ocdmStatus) {
    switch (ocdmStatus) {
        case OCDM_USABLE: return KeyStatus::USABLE;
        case OCDM_EXPIRED: return KeyStatus::EXPIRED;
        case OCDM_OUTPUT_DOWNSCALED: return KeyStatus::OUTPUT_DOWNSCALED;  // NEW
        case OCDM_USABLE_IN_FUTURE: return KeyStatus::USABLE_IN_FUTURE;     // NEW
        // ... other cases
    }
}
```

#### 5.3 Error Status Addition

```cpp
// NEW in MediaCommon.h
enum class KeyStatusErrorCode {
    SESSION_NOT_FOUND,
    INVALID_SESSION_ID,
    UNSUPPORTED_KEY_SYSTEM,
    OTHER_ERROR
};
```

**Files Affected**:
- `rialto/media/public/include/MediaCommon.h` (ADD enum values)
- `rialto/media/public/include/IMediaKeys.h` (Documentation)
- `rialto/proto/mediakeysmodule.proto` (ADD proto enums)
- `rialto/wrappers/opencdm/` (FIX conversion functions)

### Testing Requirements

| Test | Coverage |
|------|----------|
| Enum mapping | All values convert correctly |
| OCDM roundtrip | OCDM → Rialto → OCDM preservation |
| W3C compliance | EME spec adherence |
| Error handling | SESSION_NOT_FOUND behavior |

### Use Cases

1. **W3C EME compliance** - Spec-compliant key status
2. **HDCP compliance** - OUTPUT_DOWNSCALED handling
3. **License timing** - USABLE_IN_FUTURE scenarios
4. **Error recovery** - SESSION_NOT_FOUND handling

---

## Phase 6: Device Individualization (§7) - OPTIONAL

**Timeline**: 2-3 days  
**Complexity**: Medium  
**Effort**: Medium  
**Priority**: Low (Optional)  
**Dependencies**: None  
**Status**: ⚠️ **NOT MANDATORY** for V1.1 conformance

### What Changes

#### 6.1 Individualization Callback

```cpp
class IMediaKeysClient {
public:
    // NEW
    virtual void onIndividualizationRequest(const std::string &challengeData) = 0;
};
```

#### 6.2 Device Provisioning Flow

```
Server detects need → Challenge generated → 
onIndividualizationRequest(challenge) → 
Client handles provisioning → 
Response sent back → Server updates device
```

#### 6.3 Implementation

**Files Affected**:
- `rialto/media/public/include/IMediaKeysClient.h` (ADD callback)
- `rialto/proto/mediakeysmodule.proto` (ADD IndividualizationRequestEvent)
- `rialto/media/server/mediakeysserver/` (Emit events)
- `rialto-gstreamer/source/` (Implement handler)

### Rationale

- Optional: Not required for basic DRM
- Device-specific: Only needed if provisioning required
- Deferrable: Can be added later without breaking changes

---

## Phase 7: Persistent Licenses (§9)

**Timeline**: 4-6 days  
**Complexity**: High  
**Effort**: High  
**Priority**: Medium (Depends on use case)  
**Dependencies**: None (Phase 6 independent)

### What Changes

#### 7.1 Session Persistence

```cpp
// NEW in IMediaKeys
bool loadSession(int32_t mediaKeySessionId, 
                 const std::string &persistentSessionId);
```

#### 7.2 License Release Callback

```cpp
class IMediaKeysClient {
public:
    // NEW
    virtual void onLicenseRelease(const LicenseReleaseEvent &event) = 0;
};

struct LicenseReleaseEvent {
    std::string persistentSessionId;
    int32_t mediaKeySessionId;
    LicenseReleaseReason reason;
};
```

#### 7.3 Cross-Process Restore

**Problem**: Process crash loses license context

**Solution**:
1. Save `persistentSessionId` on device
2. On recovery, call `loadSession(sessionId, persistentId)`
3. Server restores license state
4. Playback resumes with same license

#### 7.4 Implementation

**Files Affected**:
- `rialto/media/public/include/IMediaKeys.h` (ADD loadSession)
- `rialto/media/public/include/IMediaKeysClient.h` (ADD onLicenseRelease)
- `rialto/media/public/include/MediaCommon.h` (NEW LicenseReleaseEvent)
- `rialto/proto/mediakeysmodule.proto` (ADD persistentSessionId, LicenseReleaseEvent)
- `rialto/media/server/mediakeysserver/` (Implement persistence)
- `rialto-gstreamer/source/` (Handle release events)

### Testing Requirements

| Test | Coverage |
|------|----------|
| Save/restore cycle | Persistent ID roundtrip |
| License validity | Post-restore license works |
| Process crash recovery | Session survives restart |
| Release acknowledgement | Event tracking |
| Concurrent sessions | Multiple persistent IDs |

### Use Cases

1. **Process crash recovery** - Resume playback after crash
2. **App backgrounding** - Pause/resume without losing license
3. **Multi-instance playback** - Coordinate across processes
4. **License lifecycle** - Explicit release handling

---

## Phase 8: Application Lifecycle Reclaim (§10)

**Timeline**: 5-7 days  
**Complexity**: High  
**Effort**: High  
**Priority**: Medium (Depends on lifecycle needs)  
**Dependencies**: Phase 7 (implicit dependency on session state)

### What Changes

#### 8.1 Generic Reclaim Callback

```cpp
class IMediaPipelineClient {
public:
    virtual void notifyReclaimed(ResourceType type, ReclaimReason reason) = 0;
};

class IWebAudioPlayerClient {
public:
    virtual void notifyReclaimed(ResourceType type, ReclaimReason reason) = 0;
};

class IMediaKeysClient {
public:
    virtual void notifyReclaimed(ResourceType type, ReclaimReason reason) = 0;
};
```

#### 8.2 Resource Types

```cpp
enum class ResourceType {
    MEDIA_PIPELINE,
    WEB_AUDIO_PLAYER,
    MEDIA_KEYS_SESSION,
    GPU_MEMORY,
    SYSTEM_MEMORY,
    CODEC_DECODER,
    OTHER
};

enum class ReclaimReason {
    RECOVERED_RESOURCE,     // Resource was freed
    MEMORY_PRESSURE,        // Low memory condition
    LOW_BATTERY,            // Battery saving
    SHUTDOWN,               // System shutdown
    APP_BACKGROUNDING,      // App sent to background
    OTHER
};
```

#### 8.3 Lifecycle State Machine

```
[ACTIVE] → (memory pressure) → [NOTIFIED] → (app acknowledges) → [RECLAIMED]
   ↓                                              ↓
   └──→ (quick recovery) ──→ [ACTIVE]           [CLEANUP]
```

#### 8.4 Application Manager Integration

**Coordination points**:
- Serialize activation/inactivation across processes
- Track resource ownership
- Coordinate cleanup
- Handle timeouts

#### 8.5 Implementation

**Files Affected**:
- `rialto/media/public/include/IMediaPipelineClient.h` (ADD notifyReclaimed)
- `rialto/media/public/include/IWebAudioPlayerClient.h` (ADD notifyReclaimed)
- `rialto/media/public/include/IMediaKeysClient.h` (ADD notifyReclaimed)
- `rialto/media/public/include/MediaCommon.h` (NEW ResourceType, ReclaimReason)
- `rialto/proto/module.proto` (NEW ReclaimEvent)
- `rialto/media/server/main/MediaPipelineServer.h` (Implement logic)
- Application Manager integration

### Testing Requirements

| Test | Coverage |
|------|----------|
| Reclaim lifecycle | Full state machine |
| Memory pressure | Triggers correctly |
| Recovery | Resource restoration |
| Concurrent reclaims | Multiple resource types |
| App backgrounding | Lifecycle coordination |
| Timeout handling | Cleanup on timeout |

### Use Cases

1. **Memory pressure recovery** - Free memory on low-memory conditions
2. **Battery optimization** - Pause resources on low battery
3. **Application backgrounding** - Suspend inactive apps
4. **System shutdown** - Graceful cleanup
5. **Resource contention** - Coordinate between multiple players

---

## Complete Implementation Path

### Quick Path (1-2 days): Phase 1 Only
```
Phase 1: Media Metrics ✅
└─ Ready for rialto-gstreamer V1.1-RG-1/2
```

### Production Path (3.5 days): Phase 1 + 2
```
Phase 1: Media Metrics ✅
Phase 2: Metric Optimization ⏳
└─ High-frequency metric scenarios
```

### Core Features Path (18-24 days): Phases 1,2,4,5,6
```
Phase 1: Media Metrics ✅
Phase 2: Metric Optimization ⏳ 2.5 days
Phase 4: Audio Discontinuity ⏳ 3-5 days
Phase 5: KeyStatus Parity ⏳ 1-2 days (QUICK!)
Phase 6: Individualization ⏳ 2-3 days (Optional)
└─ Baseline V1.1 for most applications
```

### Full V1.1 Path (25-40 days): All Phases
```
Phase 1: Media Metrics ✅
Phase 2: Metric Optimization ⏳ 2.5 days
Phase 3: Video Capture ⏳ 7-10 days (COMPLEX!)
Phase 4: Audio Discontinuity ⏳ 3-5 days
Phase 5: KeyStatus Parity ⏳ 1-2 days (QUICK!)
Phase 6: Individualization ⏳ 2-3 days (Optional)
Phase 7: Persistent Licenses ⏳ 4-6 days
Phase 8: App Lifecycle ⏳ 5-7 days
└─ Complete V1.1 feature set
```

---

## Recommended Next Steps

1. **Phase 2** - Low effort, high impact for production scenarios
2. **Phase 5** - Quick win, W3C compliance, 1-2 days
3. **Phase 4** - Medium effort, content transition support
4. **Phase 3** - Plan carefully, highest complexity
5. **Phases 7-8** - Required for full lifecycle management

---

**Choose your implementation path based on requirements and timeline.**
