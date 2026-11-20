# Playback Metrics - QA Testing Guide

## What Are We Testing?

This document explains how to verify that **three metric events are being sent periodically** during video playback:
1. **Frame Drop Events** - Sent every 100 polling cycles
2. **Frame Repeat Events** - Sent every 80 polling cycles  
3. **Decode Error Events** - Sent every 120 polling cycles

These events are logged and can be monitored through GStreamer debug logs.

---

## Setup Instructions

### Step 1: Prepare Your Test Environment

```bash
# Create a test directory
mkdir -p ~/metric_tests
cd ~/metric_tests

# Set up debug logging
export GST_DEBUG=westerosSink*:5
export GST_DEBUG_FILE=metrics_test.log

# Optional: Monitor logs in real-time in another terminal
tail -f metrics_test.log | grep -E "metric|drop|repeat|error"
```

### Step 2: Prepare Test Video

Use any MP4 or H.264 video file (minimum 60 seconds recommended):
```bash
# Example: Use an existing test video
gst-launch-1.0 filesrc location=/path/to/test.mp4 ! qtdemux ! h264parse ! [sink]
```

---

## Three Main Test Cases

### TEST 1: Basic Metric Posting (Normal Playback)

**Purpose:** Verify metrics are posted at regular intervals during normal playback.

**Steps:**
1. Play a 60-second video file continuously without pausing
2. Let playback complete
3. Stop the pipeline

**What to Check:**

| Metric | Expected Count | Tolerance | Log Pattern |
|--------|----------------|-----------|------------|
| **Frame Drop** | ~600 events | ±60 events (±10%) | `Posted synthetic METRIC_VIDEO_FRAME_DROP` |
| **Frame Repeat** | ~750 events | ±75 events (±10%) | `Posted synthetic METRIC_VIDEO_FRAME_REPEAT` |
| **Decode Error** | ~500 events | ±50 events (±10%) | `Posted synthetic METRIC_VIDEO_DECODE_ERROR` |

**How to Count (from log file):**
```bash
# Count Frame Drop events
grep "Posted synthetic METRIC_VIDEO_FRAME_DROP" metrics_test.log | wc -l

# Count Frame Repeat events
grep "Posted synthetic METRIC_VIDEO_FRAME_REPEAT" metrics_test.log | wc -l

# Count Decode Error events
grep "Posted synthetic METRIC_VIDEO_DECODE_ERROR" metrics_test.log | wc -l
```

**Pass Criteria:**
- ✅ Frame Drop count between 540-660 events
- ✅ Frame Repeat count between 675-825 events
- ✅ Decode Error count between 450-550 events
- ✅ No error messages in logs (search for "unable to post")

**Fail Criteria:**
- ❌ Counts outside expected range
- ❌ Error messages: `"unable to post media-pipeline-metric"`
- ❌ Logs show `"gst_structure_new failed"` or similar errors

---

### TEST 2: Pause and Resume

**Purpose:** Verify metrics continue being posted even during pause.

**Steps:**
1. Play a 60-second video
2. After 10 seconds: **PAUSE** playback
3. Wait 10 seconds while paused
4. **RESUME** playback
5. Let it complete

**What to Check:**

Before Pause (first 10 seconds):
```bash
grep "Posted synthetic" metrics_test.log | head -1
# Note: What line number this appears at
```

During Pause (wait 10 seconds):
```bash
# Check if more metrics were posted while paused
grep "Posted synthetic" metrics_test.log | sed -n '50,150p'
# Look for timestamps progressing even during pause
```

**Expected Behavior:**
- Metrics continue posting during pause (not frozen)
- Timestamps keep incrementing
- Resume continues from where pause occurred

**Pass Criteria:**
- ✅ Events found in log while paused
- ✅ Timestamps increase during pause
- ✅ No gaps in metric posting
- ✅ Resume continues smoothly

**Fail Criteria:**
- ❌ No events during pause (metrics frozen)
- ❌ Timestamps don't increment during pause
- ❌ Large gaps in log entries

---

### TEST 3: Seek Operations

**Purpose:** Verify metrics handle seeking correctly.

**Steps:**
1. Play video for 10 seconds
2. **SEEK** to 30 seconds (or any position)
3. Continue playback for 10 more seconds
4. Stop

**What to Check:**

Before Seek:
```bash
grep "metric.*pts-ns" metrics_test.log | head -5
# Note the timestamp (pts-ns value)
```

After Seek:
```bash
# Seek should show a jump in PTS values
grep "metric.*pts-ns" metrics_test.log | tail -5
# Should show much higher timestamp values
```

**Expected Behavior:**
- Timestamps (PTS) jump to new position after seek
- Metrics continue posting after seek
- No duplicate or out-of-order timestamps

**Pass Criteria:**
- ✅ PTS values jump after seek
- ✅ Metrics resume immediately after seek
- ✅ No backward timestamp jumps
- ✅ No duplicate events at boundary

**Fail Criteria:**
- ❌ PTS doesn't change after seek
- ❌ Metrics stop after seek
- ❌ Timestamps go backwards
- ❌ Gaps in metric posting after seek

---

## How to Read the Logs

### Finding Metric Events

Each metric event appears in the log with this pattern:
```
Posted synthetic METRIC_VIDEO_FRAME_DROP at PTS: 1234567890 ns
Posted synthetic METRIC_VIDEO_FRAME_REPEAT at PTS: 2345678901 ns
Posted synthetic METRIC_VIDEO_DECODE_ERROR at PTS: 3456789012 ns
```

### Quick Log Analysis Commands

```bash
# 1. See all metric posts in order
grep "Posted synthetic" metrics_test.log | tail -50

# 2. Count each type
echo "Frame Drop Events:"
grep "METRIC_VIDEO_FRAME_DROP" metrics_test.log | wc -l
echo "Frame Repeat Events:"
grep "METRIC_VIDEO_FRAME_REPEAT" metrics_test.log | wc -l
echo "Decode Error Events:"
grep "METRIC_VIDEO_DECODE_ERROR" metrics_test.log | wc -l

# 3. Check for errors
grep -i "error\|failed\|unable" metrics_test.log | grep -v "DECODE_ERROR"

# 4. See timestamp progression
grep "Posted synthetic" metrics_test.log | awk '{print $NF}' | head -20

# 5. Find gaps (timestamps should increase)
grep "Posted synthetic" metrics_test.log | head -100 | tail -20
```

### Understanding Metric Structure

The actual GStreamer message looks like:
```
media-pipeline-metric:
  metric: 1 (or 3, or 5)
  observation-kind: 1 (OCCURRENCE)
  pts-ns: 12345678
  source_id: 0
  session_id: 1
```

**What Each Field Means:**
- **metric 1** = Frame Drop event
- **metric 3** = Decode Error event
- **metric 5** = Frame Repeat event
- **observation-kind 1** = Single event (normal for our metrics)
- **pts-ns** = Presentation timestamp in nanoseconds (should increase)

---

## Troubleshooting Guide

### Problem: "No metric events in logs"

**Check:**
```bash
# 1. Verify log file exists and has content
ls -la metrics_test.log
wc -l metrics_test.log

# 2. Check if debug flag is working
grep "westerosSink" metrics_test.log | head -5

# 3. Ensure video played long enough (60+ seconds recommended)
```

**Solution:**
- Verify `GST_DEBUG` is set correctly
- Ensure video file is valid and played completely
- Check if decoder is being used (may fall back to software decode)

---

### Problem: "Event counts way too low or too high"

**Check:**
```bash
# 1. Verify log file size (should grow during 60s playback)
ls -lh metrics_test.log

# 2. Check for error messages
grep -i "unable\|error" metrics_test.log | head -10

# 3. See event interval pattern
grep "Posted synthetic" metrics_test.log | awk '{print $4}' | head -20
```

**Solution:**
- Video may not have played full duration
- Decoder may have errors (check log for "ERROR" lines)
- Verify test video is not damaged or corrupted

---

### Problem: "Timestamps go backwards"

**Check:**
```bash
# Extract PTS values and check ordering
grep "Posted synthetic" metrics_test.log | \
  sed 's/.*PTS: //' | \
  sed 's/ ns.*//' | \
  head -20
```

**Expected:**
```
1000000
2000000
3000000  ← Always increasing
4000000
```

**If you see:**
```
1000000
2000000
1500000  ← Going backwards = PROBLEM
```

**Solution:**
- Video file may be corrupted
- Seek/restart during test caused reset
- Try with a different video file

---

## Expected Output Samples

### Sample Log Output (Good)
```
[Westeros] updateMetrics called - decoder active
[Westeros] Posted synthetic METRIC_VIDEO_FRAME_DROP at PTS: 123456789 ns
[Westeros] Posted synthetic METRIC_VIDEO_FRAME_REPEAT at PTS: 223456789 ns
[Westeros] Posted synthetic METRIC_VIDEO_DECODE_ERROR at PTS: 323456789 ns
[Westeros] updateMetrics called - decoder active
[Westeros] Posted synthetic METRIC_VIDEO_FRAME_DROP at PTS: 423456789 ns
[Westeros] Posted synthetic METRIC_VIDEO_FRAME_REPEAT at PTS: 523456789 ns
[Westeros] Posted synthetic METRIC_VIDEO_DECODE_ERROR at PTS: 623456789 ns
```

### Sample Event Count (Good - 60 second test)
```
Frame Drop Events: 598
Frame Repeat Events: 752
Decode Error Events: 498
Total: 1848 events
```

### Sample Error Output (Bad)
```
[Westeros] unable to post media-pipeline-metric type 1
[Westeros] gst_element_post_message returned FALSE
[Westeros] metrics posting failed
```

---

## Quick Reference: Intervals Explained

The system sends metrics based on **how many times the decoder is polled**, not on actual frame count:

- **Every 100 polls** → Send a Frame Drop event
- **Every 80 polls** → Send a Frame Repeat event
- **Every 120 polls** → Send a Decode Error event

In a 60-second test with ~1000 polls per second:
- Total polls ≈ 60,000
- Frame Drop events ≈ 60,000 / 100 = **600 events**
- Frame Repeat events ≈ 60,000 / 80 = **750 events**
- Decode Error events ≈ 60,000 / 120 = **500 events**

---

## Sign-Off Checklist

- [ ] TEST 1: Basic Metric Posting - PASS / FAIL
- [ ] TEST 2: Pause/Resume - PASS / FAIL
- [ ] TEST 3: Seek Operations - PASS / FAIL
- [ ] No error messages in logs - YES / NO
- [ ] Event counts within ±10% tolerance - YES / NO
- [ ] Timestamps monotonic (always increasing) - YES / NO

**Overall Status:** ☐ PASS ☐ FAIL

**Notes / Issues Found:**
```
_________________________________
_________________________________
_________________________________
```

**Tested By:** ___________________  
**Date:** _______________________
