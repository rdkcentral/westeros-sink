# Changelog

All notable changes to westeros-sink are documented in this file.

## [2.1.2] - 2026-07-13
**Tag:** 2.1.2

### Changed
- Raw video handling inside westeros-sink soc with DRM authentication
- Bringing low memory mode functionality

## [2.1.1] - 2026-04-27
**Tag:** 2.1.1

### Added
- Enable secure video when the video decoder can switch between secure and unsecure

### Changed
- Add the changes needed for open-sourcing
- westeros-soc-brcm: update Dolby Vision dynamic range mode

## [2.1.0] - 2026-02-24
**Tag:** 2.1.0

### Added
- Added logging of version

### Fixed
- Fix regression affecting EOS detection
- Coverity defects of westeros-sink

## [2.0.0] - 2026-01-10
**Tag:** Westeros-2.0.0

### Notes
- From this release onwards, Westeros is separated into different repos.

### Fixed
- Westeros-sink reporting pre-seek position after seek, causing playback position jumps on video-only playback
- Fix westeros-sink `timeCodeFound` function
- westeros-soc-brcm: Ignore playback rate 0.25–2.0 when audio is passthrough

## [1.01.62] - 2025-10-28
**Tag:** Westeros-1.01.62

### Fixed
- v4l2: Fix frame dropping boundary condition for seek accuracy
- v4l2: Fix compile error on platforms without `V4L2_PIX_FMT_AV1` defined
- v4l2: Fix thread race condition causing video decode crashes
- v4l2: Update video decode error handling
- brcm: Increase EOS "safety net" timeout (previously too short for I-frame only streams such as REW)

## [1.01.61] - 2025-09-30
**Tag:** Westeros-1.01.61

### Fixed
- essos: Blacklist status; fix revoke defect

## [1.01.60] - 2025-09-10
**Tag:** Westeros-1.01.60

### Added
- v4l2: Add low-latency-mode for Netflix DPI 7.0 support

### Fixed
- brcm: Fix `NXCLIENT_BAD_SEQUENCE_NUMBER` error when leaving Netflix Dolby Vision
- brcm: Add check for `stc_channel == 0` to reduce error logging during gaming/low latency

---

## Dependencies

| Dependency       | Minimum Version |
|------------------|-----------------|
| wayland          | >= 1.6.0        |
| libxkbcommon     | >= 0.8.3        |
| xkeyboard-config | >= 2.18         |
| gstreamer        | >= 1.10.4       |
| EGL              | >= 1.4          |
| GLES             | >= 2.0          |
