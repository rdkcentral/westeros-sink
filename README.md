# westeros
Wayland Compositor

Westeros is a light-weight Wayland compositor library. It uses the Wayland protocols, and is designed
to be compatible with applications built to use Wayland compositors. It implements a library that
enables an application to create one or more Wayland displays. It supports the creation of normal, nested, and
embedded wayland compositors.  A normal compositor displays its composited output to the screen, while a nested
compositor sends its composited output to another compositor as a client surface.  An embedded compositor allows
the application that has created the embedded Wayland compositor to incorporate its composited output into the
applications UI.  This allows for easy integration of the UI of external third party applications into an
applications's UI.

---
# Temporary caption visibility diagnostics

Each added `cc_test sink` message is emitted to stderr using `fprintf` and
`fflush`, and through `GST_DEBUG_OBJECT` in the `westerossink` category.
The latter requires the hosting process to enable that category at DEBUG
level (for example, `GST_DEBUG=westerossink:5`); stderr capture and GStreamer
logging configuration depend on the application.

- `surface_id` identifies the sink, shell connection, Wayland surface and
  assigned protocol ID at registration.
- `shell_status` records only status events that change `sink->visible` while
  `show` is enabled. `ownId`, `eventId`, `match` and `name` identify whether
  another surface's status is being applied. Unchanged visibility and events
  while `show` is disabled do not produce this diagnostic.
- Broadcom `show_property` records explicit `show-video-window` property calls,
  including the old/new show value and current cached visibility.
- Broadcom `nexus_visible` records the existing visibility writes at video
  acquisition, property updates and position updates. `setRc` is the existing
  setter's return value, not a hardware readback. The transition's old value
  comes from the existing settings query. `geometryChanged=0` shows that a
  visibility write can occur without a geometry change.

There are no new per-frame logs, timers, counters, state fields, locks, API calls
to the compositor or hardware, or ID filters. Existing visibility assignments,
settings queries and writes are unchanged; setter results are captured only
for logging. A missing `nexus_visible` message does not establish a hardware
state: the update may not have run, may have taken the software-decode return,
may have no video window, or may have found matching visibility already.
If both logging channels are captured, duplicate messages are expected.

To investigate, correlate a caption `shell_status` with `match=0` and
`visible=1->0` with a subsequent `nexus_visible source=update_position` for the
same sink and video window. Also check the reverse transition on caption show
and whether a `show_property` call explains the write instead. This establishes
the receiver/write sequence, not successful scanout or a definitive root cause.

---
# Copyright and license

Westeros sink is licensed under the LGPLv2.1 license. 

