# westeros
Wayland Compositor

## Broadcom Video Metric Messages

The Broadcom sink posts `GST_MESSAGE_ELEMENT` messages with the structure name
`media-pipeline-metric`. Each message contains `metric` and `observation-kind`
as `G_TYPE_UINT`; optional `pts-ns` is `G_TYPE_INT64`. These names, types and
values follow the [SoC media metrics specification](../SPECIFICATION-soc-media-metrics%20(3).md).
The message source is the producing sink element. Existing `source_id` and
`session_id` fields are optional integration metadata; they do not replace the
authoritative GStreamer message source.

This video sink emits the applicable video-drop, video-decode-error,
video-repeat and decoder-underflow metrics; audio metrics are owned by the
corresponding audio-path elements.

| Metric | `metric` | `observation-kind` |
|---|---:|---|
| Video frame drop | 1 | 1 (OCCURRENCE) |
| Video decode error | 3 | 1 (OCCURRENCE) |
| Video frame repeat | 5 | 2 (EPISODE_STARTED), 3 (EPISODE_RESOLVED) |
| Buffer underflow | 7 | 2 (EPISODE_STARTED), 3 (EPISODE_RESOLVED) |

Resolved messages contain `duration-ns` (`G_TYPE_UINT64`); video-repeat
resolutions also contain `count` (`G_TYPE_UINT64`). Both repeat and underflow
resolutions retain the episode-start `pts-ns`. Occurrences contain no summary
fields. The legacy buffer-underflow signal and upstream event are unchanged.

The readable `stats` property returns one `GstStructure` snapshot containing
`rendered`, `dropped` and `corrupted`, all `G_TYPE_UINT64`. The snapshot uses
the sink's cumulative metric counters and remains monotonic across flushes and
decoder restarts. It returns unavailable (`NULL`) after frame capture hands
final presentation to the application renderer.

The sink expands sampled native counter deltas into individual occurrence
messages. The status loop sleeps for 8 ms between samples; native `guint`
counters use unsigned delta arithmetic, with baselines reset at decoder
flush/restart boundaries. The Broadcom SDK must confirm native reset behavior
outside those boundaries and the maximum event rate against the actual sampling
interval. Episode duration uses monotonic time. Pause resolves video-repeat but
retains decoder underflow while demand continues; flush, EOS, resource loss and
teardown resolve the episodes owned by the affected decoder/output path.

The Broadcom SDK currently exposes sampled counters rather than frame-identity
callbacks. Consequently, the sink cannot independently prove frame-level
deduplication, distinguish all structured cadence from unintended repeats, or
observe repeat presentation boundaries between status polls. The native FIFO
empty callback is also unavailable in immediate-output mode. Those cases still
require platform observations to meet full SoC-spec conformance.

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

