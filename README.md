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
- `shell_status action=received` records every status event before filtering,
  including unchanged visibility and events while `show` is disabled.
- `shell_status action=apply` records matching-surface status events that
  change `sink->visible` while `show` is enabled.
- `shell_status action=ignore` records another surface's status when its
  visibility differs from the sink's cached visibility and `show` is enabled.
  `ownId`, `eventId`, `match` and `name` identify the surfaces. These events
  do not change the sink's visibility, opacity, z-order or `windowChange`.
  Unchanged visibility and events while `show` is disabled do not produce
  this diagnostic.
- Broadcom `show_property` records explicit `show-video-window` property calls,
  including the old/new show value and current cached visibility.
- Broadcom `nexus_visible` records the existing visibility writes at video
  acquisition, property updates and position updates. `setRc` is the existing
  setter's return value, not a hardware readback. The transition's old value
  comes from the existing settings query. `geometryChanged=0` shows that a
  visibility write can occur without a geometry change.

Simple Shell broadcasts status for other clients' surfaces. The sink now
ignores status whose surface ID does not match its own; matching-surface
handling and the existing `show` gate are unchanged.

There are no new per-frame logs, timers, counters, state fields, locks or API
calls to the compositor or hardware. Existing backend settings queries and
writes are unchanged; setter results are captured only for logging.
A missing `nexus_visible` message does not establish a hardware
state: the update may not have run, may have taken the software-decode return,
may have no video window, or may have found matching visibility already.
If both logging channels are captured, duplicate messages are expected.

On CC off, expect a caption `shell_status` with `match=0`, `eventVisible=0`
and `action=ignore`, with no caption-induced Nexus visibility change. On CC on,
expect `action=received` with `match=0` and `eventVisible=1`; an additional
`action=ignore` is only logged if its visibility differs from the sink's.
Verify that video stays visible while captions hide/show, and that status
targeting the sink's own surface and `show-video-window` still work.

---
# Copyright and license

Westeros sink is licensed under the LGPLv2.1 license. 

