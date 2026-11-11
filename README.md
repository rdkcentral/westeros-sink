# westeros
Wayland Compositor

## Broadcom media metrics (SoC Media Metrics GStreamer Specification v0.7)

The Broadcom sink owns the video decoder and the video output, so it posts `media-pipeline-metric`
element messages (`brcm/westeros-sink-metrics.c`):

| Observation | `metric` | `observation-kind` | Extra fields |
|---|---|---|---|
| Video frame drop (decode + display drops inside the segment) | 1 | 1 occurrence | `pts-ns` |
| Video decode error | 3 | 1 occurrence | `pts-ns` |
| Video frame repeat (display underflow until a new PTS is shown) | 5 | 2 started / 3 resolved | `pts-ns`; resolved: `duration-ns`, `count` |
| Video buffer underflow (FIFO-empty callback until refill) | 7 | 2 started / 3 resolved | `pts-ns`; resolved: `duration-ns` |

Fields are `G_TYPE_UINT` (`metric`, `observation-kind`), `G_TYPE_INT64` (`pts-ns`, episode-start PTS for
episodes) and `G_TYPE_UINT64` (`duration-ns` from `gst_util_get_timestamp()`, `count`). Each native counter
increment posts one occurrence. Pause and frame-capture handoff resolve the repeat episode; flush, stop,
resource loss, EOS and teardown resolve every active episode before the decoder stops. Drops while a new
decode position is established are not reported. Messages are posted after the sink lock is released.

The `stats` property returns monotonic `rendered`, `dropped` and `corrupted` (`guint64`) counters that use the
same identity as the drop and decode-error occurrences and are not reset by flush. `corrupted` is omitted
while frame capture is active, because final presentation is downstream.

Counters are sampled at the frame poll interval, so repeats between polls are attributed to the next sample.
Host test: `cd brcm/test && gcc -I.. westeros-sink-metrics-test.c ../westeros-sink-metrics.c $(pkg-config --cflags --libs gstreamer-1.0) && ./a.out`

Westeros is a light-weight Wayland compositor library. It uses the Wayland protocols, and is designed
to be compatible with applications built to use Wayland compositors. It implements a library that
enables an application to create one or more Wayland displays. It supports the creation of normal, nested, and
embedded wayland compositors.  A normal compositor displays its composited output to the screen, while a nested
compositor sends its composited output to another compositor as a client surface.  An embedded compositor allows
the application that has created the embedded Wayland compositor to incorporate its composited output into the
applications UI.  This allows for easy integration of the UI of external third party applications into an
applications's UI.

---
# Copyright and license

Westeros sink is licensed under the LGPLv2.1 license. 

