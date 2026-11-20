# Phase 2-8 Quick Summary

## Phase 2: Metric Optimization (2.5 days) 📊

**What**: Optimize metric delivery for high-frequency scenarios
- Per-client subscription filtering
- Event batching (reduce RPC overhead)
- Performance tuning

**Why**: Production scenarios with 1000+ metrics/second

**Files**: rialto-gstreamer client only

---

## Phase 3: Video Frame Capture (7-10 days) ⚠️ COMPLEX

**What**: GPU texture/image capture for rendering consumers
- GL texture capture
- Vulkan image capture
- Frame pool management
- DMA-BUF support
- Color metadata

**Why**: Netflix VaaT V2, texture-based rendering, PGIO

**Files**: New IVideoFrameCapture interface, SoC HAL integration

**⚠️ WARNING**: Highest complexity phase - requires deep GPU expertise

---

## Phase 4: Audio Discontinuity (3-5 days) 🔊

**What**: Metadata-driven audio gap handling
- AudioDiscontinuity structure
- Attach to audio segments
- Deprecate processAudioGap()
- GStreamer routing

**Why**: Program transitions, ad breaks, audio sync

**Files**: Segment metadata, protocol buffers, GStreamer integration

---

## Phase 5: KeyStatus Parity (1-2 days) ⚡ QUICK WIN

**What**: W3C EME compliance updates
- OUTPUT_DOWNSCALED enum
- USABLE_IN_FUTURE enum
- Fix OCDM conversion bug
- SESSION_NOT_FOUND error

**Why**: W3C EME specification compliance

**Files**: MediaCommon.h, OCDM wrappers (small change!)

**⏱️ FASTEST PHASE**: 1-2 days, high compliance value

---

## Phase 6: Device Individualization (2-3 days) 🎯 OPTIONAL

**What**: Device provisioning callback
- onIndividualizationRequest() callback
- Challenge/response flow
- Device provisioning workflow

**Why**: Device-specific DRM provisioning (optional)

**Files**: IMediaKeysClient callback, proto events

**Note**: Optional - not mandatory for V1.1

---

## Phase 7: Persistent Licenses (4-6 days) 🔐

**What**: Cross-process license recovery
- Save/restore persistent session IDs
- onLicenseRelease() callback
- Process crash recovery
- License lifecycle management

**Why**: Survive process crashes, multi-instance playback

**Files**: IMediaKeys loadSession(), persistent ID tracking

---

## Phase 8: App Lifecycle Reclaim (5-7 days) ♻️

**What**: Generic resource reclamation framework
- notifyReclaimed() callback
- Memory pressure handling
- Battery optimization
- Shutdown coordination

**Why**: Resource efficiency, lifecycle management

**Files**: IMediaPipelineClient/WebAudioPlayerClient/MediaKeysClient callbacks

---

## Implementation Paths

### Path A: Metrics Only (1 day) ✅ DONE
Phase 1 → rialto-gstreamer V1.1-RG-1/2

### Path B: Production Metrics (3.5 days)
Phase 1 + Phase 2 → High-frequency metrics support

### Path C: Core V1.1 (18-24 days)
Phases 1,2,4,5,6 → Baseline V1.1 features

### Path D: Full V1.1 (25-40 days)
All phases → Complete implementation

---

## Complexity Breakdown

**🟢 Low Complexity** (Easy):
- Phase 5: KeyStatus Parity (1-2 days)
- Phase 1: Media Metrics (DONE)

**🟡 Medium Complexity**:
- Phase 2: Metric Optimization (2.5 days)
- Phase 4: Audio Discontinuity (3-5 days)
- Phase 6: Individualization (2-3 days, optional)

**🔴 High Complexity**:
- Phase 7: Persistent Licenses (4-6 days)
- Phase 8: App Lifecycle (5-7 days)

**🔴🔴 VERY HIGH Complexity** (HARDEST):
- Phase 3: Video Capture (7-10 days)

---

## Recommended Priority

1. **Phase 2** - Medium effort, high production impact
2. **Phase 5** - Quick win, compliance value
3. **Phase 4** - Content features
4. **Phase 3** - Plan carefully (very complex)
5. **Phases 7-8** - Full lifecycle management

---

For detailed information, see: **PHASES-2-TO-8-DETAILED-GUIDE.md**
