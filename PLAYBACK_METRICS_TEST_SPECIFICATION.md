# Playback Metrics - Complete Test Specification

**Document Version:** 2.0 (QA-Friendly)  
**Date:** 2026-11-19  
**Component:** Westeros Sink Video Decoder (Broadcom BRCM)  
**Purpose:** Test guide for metric event generation during video playback

---

## 1. What You Need to Know (Intro for QA)

### The Goal
Verify that the video decoder sends **metric events** at regular intervals during playback:
- **Frame Drop Event** → Every 100 decoder polls
- **Frame Repeat Event** → Every 80 decoder polls  
- **Decode Error Event** → Every 120 decoder polls

These events are logged as GStreamer messages and can be monitored using debug logs.

### Key Points
- Tests should run on actual hardware or testbed
- Metrics are **poll-based**, not frame-based (more consistent)
- Events post even during pause/seek (synthetic generation)
- Logs are your primary verification method

---

## 2. The Three Metric Events (What We're Testing)

### 2.1 Event Types We Care About

We only test **3 metric events** that post at regular intervals:

| # | Event Name | Log Pattern | Interval | Expected Count (60s) |
|---|---|---|---|---|
| 1 | **Frame Drop** | `Posted synthetic METRIC_VIDEO_FRAME_DROP` | Every 100 polls | ~600 events |
| 2 | **Frame Repeat** | `Posted synthetic METRIC_VIDEO_FRAME_REPEAT` | Every 80 polls | ~750 events |
| 3 | **Decode Error** | `Posted synthetic METRIC_VIDEO_DECODE_ERROR` | Every 120 polls | ~500 events |

**That's it!** Focus on these three during testing.

### 2.2 Other Metrics (Reference Only - Not Tested Here)

There are other metric types in the system (Audio drops, underflow, audio gaps) but they are NOT part of this test scope. We only validate the three interval-based metrics above.

---

## 3. How Events Are Sent (Technical Details for Understanding)

### 3.1 Event Structure

Each metric event is a GStreamer message containing:

```
Event:
  ├─ Type: 1 (drop) or 3 (error) or 5 (repeat)
  ├─ Timestamp: When event was posted (in nanoseconds)
  ├─ Source ID: Which decoder/sink
  └─ Session ID: Which playback session
```


## 4. How to Set Up Your Test Environment

### 4.1 Enable Debug Logging

**Option A: Detailed Logging (Recommended for Testing)**
```bash
export GST_DEBUG=westerosSink*:5
export GST_DEBUG_FILE=metric_test.log
```

**Option B: Minimal Logging**
```bash
export GST_DEBUG=westerosSink*:3
export GST_DEBUG_FILE=metric_test.log
```

**Option C: Real-Time Monitoring** (in another terminal)
```bash
# Keep this running during test to see logs as they appear
tail -f metric_test.log | grep "Posted synthetic"
```


## 5. The Six Test Cases (Step-by-Step)

### TEST 1: Normal Playback Metric Generation

**What We're Testing:** Do metrics post regularly during normal video playback?

**Before You Start:**
- [ ] Debug logging enabled: `GST_DEBUG=westerosSink*:5`
- [ ] Log file path set: `GST_DEBUG_FILE=metric_test.log`
- [ ] Log file doesn't exist (or backup previous one)

**Test Steps:**

1. **Clear previous logs**
   ```bash
   rm -f metric_test.log
   ```

2. **Run pipeline for full video**
   ```bash
   export GST_DEBUG=westerosSink*:5
   export GST_DEBUG_FILE=metric_test.log
   gst-launch-1.0 -v filesrc location=test.mp4 ! qtdemux ! h264parse ! rialto.sink_soc ! fakesink async=false
   ```
   Let this play completely (should take ~60 seconds)

3. **Count metrics in log**
   ```bash
   echo "Frame Drop Events:"
   grep "Posted synthetic METRIC_VIDEO_FRAME_DROP" metric_test.log | wc -l
   
   echo "Frame Repeat Events:"
   grep "Posted synthetic METRIC_VIDEO_FRAME_REPEAT" metric_test.log | wc -l
   
   echo "Decode Error Events:"
   grep "Posted synthetic METRIC_VIDEO_DECODE_ERROR" metric_test.log | wc -l
   ```

**Expected Results:**
```
Frame Drop Events: 540-660 (target ~600)
Frame Repeat Events: 675-825 (target ~750)
Decode Error Events: 450-550 (target ~500)
```

**Pass Criteria:**
- ✅ All three counts within ranges above
- ✅ No error messages about "unable to post"
- ✅ Log file has content (not empty)
- ✅ All events have timestamps

**Fail Criteria:**
- ❌ Counts outside expected ranges
- ❌ Error messages in log
- ❌ Empty or missing log file
- ❌ Less than 100 total events

**Troubleshooting:**
If counts are too low:
- Video may not have played full duration
- Check log file size: `ls -lh metric_test.log`
- Verify debug flag took effect: `grep "Posted" metric_test.log | wc -l`

If counts are way too high:
- Unusual decoder polling rate
- Video may have looped
- Check timestamps: `grep "Posted" metric_test.log | tail -10`

---

### TEST 2: Pause and Resume

**What We're Testing:** Do metrics continue during pause and resume properly?

**Test Steps:**

1. **Start video playback**
   ```bash
   export GST_DEBUG=westerosSink*:5
   export GST_DEBUG_FILE=metric_pause_test.log
   gst-launch-1.0 -v filesrc location=test.mp4 ! qtdemux ! h264parse ! rialto.sink_soc ! fakesink async=false &
   PIPE_PID=$!
   ```

2. **Let it play for 10 seconds**
   ```bash
   sleep 10
   ```

3. **Check events posted so far**
   ```bash
   BEFORE=$(grep "Posted synthetic" metric_pause_test.log | wc -l)
   echo "Events before pause: $BEFORE"
   ```

4. **Send pause command** (Ctrl+P or use gst-player pause if available)
   ```bash
   # If using gst-launch, you may need to use gst-player for interactive control
   # Or use: gst-launch-1.0 -i (interactive mode)
   ```

5. **Wait 10 seconds (paused)**
   ```bash
   sleep 10
   ```

6. **Check events during pause**
   ```bash
   DURING=$(grep "Posted synthetic" metric_pause_test.log | wc -l)
   PAUSE_EVENTS=$((DURING - BEFORE))
   echo "Events posted during 10s pause: $PAUSE_EVENTS (should be 100-150)"
   ```

7. **Resume playback**
   ```bash
   # Send resume/play command
   ```

8. **Let it complete or stop after 10 more seconds**
   ```bash
   sleep 10
   kill $PIPE_PID
   ```

**Expected Results:**
```
Events before pause: 100-125
Events during pause: 100-150  ← Metrics should post even when paused
Events after resume: Continues at normal rate
```

**Pass Criteria:**
- ✅ Events posted during pause (between 100-150)
- ✅ No gap in metric posting at pause boundary
- ✅ Timestamps continuous throughout
- ✅ No errors in log

**Fail Criteria:**
- ❌ No events during pause (metrics frozen)
- ❌ Events stop after pause
- ❌ Timestamps jump backwards

---

### TEST 3: Seek Operation

**What We're Testing:** Do metrics handle seek/jump to different position correctly?

**Test Steps:**

1. **Start pipeline in interactive mode**
   ```bash
   export GST_DEBUG=westerosSink*:5
   export GST_DEBUG_FILE=metric_seek_test.log
   gst-launch-1.0 -i filesrc location=test.mp4 ! qtdemux ! h264parse ! rialto.sink_soc ! fakesink async=false
   ```

2. **Let it play for 10 seconds (note timestamp)**
   ```bash
   # In another terminal:
   sleep 10
   BEFORE_SEEK=$(grep "Posted synthetic" metric_seek_test.log | tail -1)
   echo "Last event before seek: $BEFORE_SEEK"
   ```

3. **Seek to different position** (e.g., skip to 30 seconds)
   ```bash
   # In gst-launch-1.0 interactive console: 
   # Ctrl+P to pause, then s to seek
   # Or use gst-player: q (quit playback), then restart at different position
   ```

4. **Check timestamp after seek**
   ```bash
   AFTER_SEEK=$(grep "Posted synthetic" metric_seek_test.log | tail -1)
   echo "Event after seek: $AFTER_SEEK"
   # Timestamp in AFTER_SEEK should be much larger than BEFORE_SEEK
   ```

5. **Continue playback for 10 more seconds**
   ```bash
   sleep 10
   ```

6. **Verify metrics resumed**
   ```bash
   TOTAL=$(grep "Posted synthetic" metric_seek_test.log | wc -l)
   echo "Total events: $TOTAL (should be 300-400 for 20s playback)"
   ```

**Expected Results:**
```
Before seek PTS: 10000000000 (10 seconds in nanoseconds)
After seek PTS: 30000000000 (30 seconds - much higher)
No events missing after seek
Metrics continue at normal rate
```

**Pass Criteria:**
- ✅ Timestamp increases significantly after seek
- ✅ Metrics resume immediately after seek
- ✅ No duplicate events at boundary
- ✅ No backward timestamp jumps

**Fail Criteria:**
- ❌ Timestamp doesn't change after seek
- ❌ Events stop after seek
- ❌ Timestamps go backwards
- ❌ Large gaps (>5 seconds) between events

---

### TEST 4: Different Video Formats

**What We're Testing:** Do metrics work with various video types?

**Test Steps:**

For each video format below, repeat TEST 1 (normal playback):

1. **H.264 (default)**
   ```bash
   # Use test.mp4 or test.h264
   # Expected: ~600, ~750, ~500 events
   ```

2. **VP9** (if supported)
   ```bash
   # Use test.vp9 or test.webm
   # Expected: ~600, ~750, ~500 events
   ```

3. **High Resolution (4K if available)**
   ```bash
   # Use test-4k.mp4
   # Expected: ~600, ~750, ~500 events
   # (counts should be same - poll-based, not frame-based)
   ```

**Expected Results:**
```
All formats should show similar event counts:
  Frame Drop: 540-660
  Frame Repeat: 675-825
  Decode Error: 450-550
```

**Pass Criteria:**
- ✅ All video formats produce metrics
- ✅ Event counts consistent across formats
- ✅ No decoder-specific errors

**Fail Criteria:**
- ❌ One format produces no metrics
- ❌ Wildly different counts between formats
- ❌ Decoder errors specific to format

---

### TEST 5: Error Detection (Corrupt Video)

**What We're Testing:** Do metrics post even with corrupted/problematic video?

**Test Steps:**

1. **Create intentionally corrupt video**
   ```bash
   # Option 1: Truncate a valid video file
   head -c 500000000 valid_video.mp4 > corrupt_video.mp4
   
   # Option 2: Use existing problematic video if available
   ```

2. **Run metric test with corrupt video**
   ```bash
   export GST_DEBUG=westerosSink*:5
   export GST_DEBUG_FILE=metric_corrupt_test.log
   timeout 30 gst-launch-1.0 -v filesrc location=corrupt_video.mp4 ! qtdemux ! h264parse ! rialto.sink_soc ! fakesink async=false
   ```

3. **Check if any metrics posted**
   ```bash
   TOTAL=$(grep "Posted synthetic" metric_corrupt_test.log | wc -l)
   ERRORS=$(grep -i "error\|failed" metric_corrupt_test.log | wc -l)
   echo "Events posted despite errors: $TOTAL"
   echo "Error messages: $ERRORS"
   ```

**Expected Results:**
```
Events posted: 100-200 (even partial playback should generate metrics)
Error messages: Some decode errors expected (but metrics continue)
Pipeline may stop early, but metrics post while running
```

**Pass Criteria:**
- ✅ Metrics post despite errors
- ✅ Error messages logged but don't crash metrics
- ✅ No "unable to post" errors (metrics system works)

**Fail Criteria:**
- ❌ No metrics posted with corrupt video
- ❌ Metrics system crashes
- ❌ "unable to post" errors

---

### TEST 6: Long-Duration Playback (Stability)

**What We're Testing:** Do metrics work reliably over extended playback?

**Test Steps:**

1. **Prepare long video or loop test**
   ```bash
   # Option 1: Use a 300+ second video
   # Option 2: Use GStreamer loop plugin to repeat short video
   ```

2. **Run extended test**
   ```bash
   export GST_DEBUG=westerosSink*:5
   export GST_DEBUG_FILE=metric_long_test.log
   # Play for 300 seconds (5 minutes)
   gst-launch-1.0 -v filesrc location=test.mp4 ! qtdemux ! h264parse ! rialto.sink_soc ! fakesink async=false &
   PIPE_PID=$!
   sleep 300
   kill $PIPE_PID
   ```

3. **Analyze metrics**
   ```bash
   DROP=$(grep "Posted synthetic METRIC_VIDEO_FRAME_DROP" metric_long_test.log | wc -l)
   REPEAT=$(grep "Posted synthetic METRIC_VIDEO_FRAME_REPEAT" metric_long_test.log | wc -l)
   ERROR=$(grep "Posted synthetic METRIC_VIDEO_DECODE_ERROR" metric_long_test.log | wc -l)
   
   echo "5-minute playback results:"
   echo "Frame Drop: $DROP (expected 2700-3300)"
   echo "Frame Repeat: $REPEAT (expected 3375-4125)"
   echo "Decode Error: $ERROR (expected 2250-2750)"
   ```

4. **Check for memory leaks or crashes**
   ```bash
   # Monitor system while running:
   # - CPU usage should be steady
   # - Memory should not grow unbounded
   # - No segfaults in dmesg
   
   dmesg | grep -i "segfault\|killed" | tail -5
   ```

**Expected Results:**
```
5-minute test (300 seconds):
  Frame Drop: 2700-3300 events
  Frame Repeat: 3375-4125 events
  Decode Error: 2250-2750 events
  No memory leaks
  No crashes
```

**Pass Criteria:**
- ✅ Events counts within range for 300 seconds
- ✅ Metrics post consistently (no gaps)
- ✅ No crash or segfault
- ✅ Memory stable (not growing)
- ✅ No "unable to post" errors

**Fail Criteria:**
- ❌ Event counts way outside range
- ❌ Crashes or segfaults
- ❌ Memory continuously growing
- ❌ Pipeline hangs or becomes unresponsive

---

## 6. Quick Log Analysis Commands

### Common Commands You'll Use

```bash
# Count Frame Drop events
grep "Posted synthetic METRIC_VIDEO_FRAME_DROP" metric_test.log | wc -l

# Count Frame Repeat events
grep "Posted synthetic METRIC_VIDEO_FRAME_REPEAT" metric_test.log | wc -l

# Count Decode Error events
grep "Posted synthetic METRIC_VIDEO_DECODE_ERROR" metric_test.log | wc -l

# Check for ANY errors
grep -i "unable\|error" metric_test.log | grep -v "DECODE_ERROR" | head -10

# See last 20 metric posts
grep "Posted synthetic" metric_test.log | tail -20

# Count total metrics
grep "Posted synthetic" metric_test.log | wc -l

# Check log file size
ls -lh metric_test.log

# See one event entry
grep "Posted synthetic" metric_test.log | head -1
```

---

## 7. Test Summary Checklist

**Use this to track your testing progress:**

- [ ] **TEST 1 (Normal Playback)** - PASS / FAIL
  - Frame Drop: _____ events (expect 540-660)
  - Frame Repeat: _____ events (expect 675-825)
  - Decode Error: _____ events (expect 450-550)
  
- [ ] **TEST 2 (Pause/Resume)** - PASS / FAIL
  - Events during pause: _____ (expect 100-150)
  
- [ ] **TEST 3 (Seek Operation)** - PASS / FAIL
  - Timestamp changed: YES / NO
  - Metrics resumed: YES / NO
  
- [ ] **TEST 4 (Different Formats)** - PASS / FAIL
  - Format 1 (H.264): _____ events
  - Format 2 (VP9): _____ events
  - Format 3 (High-res): _____ events
  
- [ ] **TEST 5 (Corrupt Video)** - PASS / FAIL
  - Metrics posted despite errors: YES / NO
  
- [ ] **TEST 6 (Long Duration)** - PASS / FAIL
  - 5-minute test completed: YES / NO
  - No crashes: YES / NO

**Overall Status:** ☐ **ALL PASS** ☐ **SOME FAIL** ☐ **BLOCKER**

**Issues/Notes:**
```
_________________________________
_________________________________
_________________________________
_________________________________
```

**Tested By:** _____________________  
**Date:** _________________________  
**Hardware/Environment:** _____________  

---

## 8. Troubleshooting Quick Guide

### "No events in log"
```bash
# Check if debug logging is working
grep "westerosSink" metric_test.log | head -5

# If nothing: GST_DEBUG may not be set correctly
# Try: GST_DEBUG=westerosSink*:5 ./test_script.sh

# Check if video played
# Look for: "GStreaming ...", "done" messages
grep -i "starting\|done\|completed" metric_test.log
```

### "Event counts are very low (< 100)"
```bash
# Check how long video played
grep "Posted synthetic" metric_test.log | head -1
grep "Posted synthetic" metric_test.log | tail -1

# Should show events spanning 60+ seconds
# If not: video didn't play full duration
```

### "Event counts are very high (> 1000 in 10s)"
```bash
# May indicate unusual polling rate
# This is OK - just document the actual rate

# Calculate actual polls per second
# If 1500 drop events in 60s = 150 polls per second (unusual but valid)
```

### "Error messages in log"
```bash
# Check for metric-specific errors
grep -i "unable to post\|gst_structure" metric_test.log

# Check for decoder errors (these are OK)
grep -i "decode error" metric_test.log | head -3

# Check for segfaults (NOT OK)
grep -i "segfault" metric_test.log
```

---

## 9. What to Report

When reporting results, include:

1. **Test Date:** 2026-11-19
2. **Hardware:** (Device model, decoder type)
3. **Video File:** (Name, resolution, duration)
4. **Environment:** (GST_DEBUG setting used)
5. **Results:**
   ```
   TEST 1: Frame Drop = XXX, Frame Repeat = XXX, Decode Error = XXX
   TEST 2: Events during pause = XXX
   TEST 3: Seek timestamp changed = YES/NO
   TEST 4: Format tests = PASS/FAIL
   TEST 5: Corrupt video = PASS/FAIL
   TEST 6: 300s stability = PASS/FAIL
   ```
6. **Issues Found:** (If any)
7. **Log Files:** (Attach metric_test.log if issues found)

---

## 10. Contact & Support

**Questions about this test spec?**
- Check Section 8: Troubleshooting
- Review expected values in each test
- Compare your log output to examples

**Bugs found?**
- Save the log file: `cp metric_test.log metric_bug_YYYYMMDD.log`
- Note exact error message
- Provide hardware/software details
- Contact: Westeros Sink metrics team
