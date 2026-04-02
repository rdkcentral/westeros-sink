# Changelog

All notable changes to westeros-sink are documented here, in descending order of release.

---

## [2.1.0] — 24 February 2026

**Tag:** `2.1.0`

### Changes
- Fix regression affecting EOS detection
- Coverity for westeros-sink
- Fix coverity defects of westeros-sink
- Added logging of version

### Dependencies
| Package | Version |
|---------|---------|
| wayland | >= 1.6.0 |
| libxkbcommon | >= 0.8.3 |
| xkeyboard-config | >= 2.18 |
| gstreamer | >= 1.10.4 |
| EGL | >= 1.4 |
| GLES | >= 2.0 |

---

## [2.0.0] — 10 January 2026

**Tag:** `Westeros-2.0.0`

> From this release onwards, Westeros is separated into different repos.

### Changes
- Westeros-sink reporting pre-seek position after seek, causing playback position jumps on video-only playback
- Fix westeros-sink timeCodeFound function
- westeros-soc-brcm: Ignore playback rate 0.25–2.0 when audio is passthrough

### Dependencies
| Package | Version |
|---------|---------|
| wayland | >= 1.6.0 |
| libxkbcommon | >= 0.8.3 |
| xkeyboard-config | >= 2.18 |
| gstreamer | >= 1.10.4 |
| EGL | >= 1.4 |
| GLES | >= 2.0 |

---

## [1.01.62] — 28 October 2025

**Tag:** `Westeros-1.01.62`

### Changes
- v4l2: Fix frame dropping boundary condition for seek accuracy
- v4l2: Fix compile error on platforms without `V4L2_PIX_FMT_AV1` defined
- v4l2: Fix thread race condition causing video decode crashes
- brcm: Increase EOS "safety net" timeout — previously too short for I-frame only streams (e.g. REW)
- v4l2: Update video decode error handling

### Dependencies
| Package | Version |
|---------|---------|
| wayland | >= 1.6.0 |
| libxkbcommon | >= 0.8.3 |
| xkeyboard-config | >= 2.18 |
| gstreamer | >= 1.10.4 |
| EGL | >= 1.4 |
| GLES | >= 2.0 |

---

## [1.01.61] — 30 September 2025

**Tag:** `Westeros-1.01.61`

### Changes
- essos: Blacklist status — fix revoke defect

### Dependencies
| Package | Version |
|---------|---------|
| wayland | >= 1.6.0 |
| libxkbcommon | >= 0.8.3 |
| xkeyboard-config | >= 2.18 |
| gstreamer | >= 1.10.4 |
| EGL | >= 1.4 |
| GLES | >= 2.0 |

---

## [1.01.60] — 10 September 2025

**Tag:** `Westeros-1.01.60`

### Changes
- v4l2: Add low-latency-mode for Netflix DPI 7.0 support
- brcm: Fix `NXCLIENT_BAD_SEQUENCE_NUMBER` error when leaving Netflix DolbyVision
- brcm: Add check for `stc_channel == 0` to reduce error logging during gaming/low latency

### Dependencies
| Package | Version |
|---------|---------|
| wayland | >= 1.6.0 |
| libxkbcommon | >= 0.8.3 |
| xkeyboard-config | >= 2.18 |
| gstreamer | >= 1.10.4 |
| EGL | >= 1.4 |
| GLES | >= 2.0 |
