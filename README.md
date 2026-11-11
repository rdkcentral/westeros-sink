# westeros
Wayland Compositor

## Broadcom Video Metric Messages

The Broadcom sink posts `media-pipeline-metric` element messages with
`metric_type` (unsigned integer), `phase` (unsigned integer), `source_id`,
`session_id`, and `pts_ns` (signed 64-bit stream PTS in nanoseconds).
Existing SoC metric type numbers are retained; they are not protobuf enum values.

| Event | metric_type | phase |
|---|---|---|
| Video frame drop | 0 | 0 (OCCURRENCE) |
| Video frame freeze start | 4 (VIDEO_FRAME_REPEAT) | 1 (STARTED) |
| Video frame freeze end | 4 (VIDEO_FRAME_REPEAT) | 2 (RESOLVED) |
| Video decode error | 2 | 0 (OCCURRENCE) |
| Video underflow start | 6 (BUFFER_UNDERFLOW) | 1 (STARTED) |
| Video underflow end | 6 (BUFFER_UNDERFLOW) | 2 (RESOLVED) |

Each decoder drop/error counter increment produces a separate occurrence message.
Occurrences expanded from one status sample may share the same current PTS.
Cumulative rendered, dropped and corrupted frame counters are not included.
Resolved freeze messages include the episode's `count` and `duration_ns` as
unsigned 64-bit values, retaining the freeze-start PTS. Resolved underflow
messages include `duration_ns` and the recovery PTS, but no count. Started and
occurrence messages carry neither summary field. Observation timestamps belong
to the Rialto collector and are not added to the SoC envelope.

Freeze starts on a display-underflow counter increment and resolves when the
sampled decoder PTS changes. Further counter increments at the same PTS extend
the episode without another Started message. Underflow starts from the native
FIFO-empty callback and resolves on input FIFO refill or resumed byte consumption.
The legacy buffer-underflow signal is unchanged. Pause, flush, stop and observed
resource loss cancel unfinished episodes rather than reporting false recovery.
Video frame capture suppresses pipeline drop/freeze events because final
presentation occurs in the application renderer; decode-error events remain.

These are sampled observations, not a fully frame-precise HAL implementation:
counter/PTS snapshots cannot attribute repeats that occur between polls or
distinguish structured cadence unless Nexus excludes it from its underflow
counter. Freeze duration is measured between detected boundaries using monotonic
time. Underflow observations depend on the native FIFO-empty callback, which is
disabled by the existing immediate-output decoder configuration. Full proposal
conformance requires authoritative renderer boundaries and starvation recovery
observations from the platform SDK in those cases.

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

