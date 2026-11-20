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

## Simple Shell surface status

The sink ignores status broadcasts for other surface IDs. Matching-surface
updates retain the existing `show` gate for visibility and update the cached
opacity, z-order, and window-change flag.

The `shellSurfaceStatus` callback logs `cc_test sink shell_status` to stderr and
GStreamer DEBUG: `action=received` for every status, `action=ignore` for foreign
visibility differences when `show` is enabled, and `action=apply` for matching
visibility changes when `show` is enabled. Foreign statuses always return
without changing sink state, even when no `action=ignore` line is emitted.

---
# Copyright and license

Westeros sink is licensed under the LGPLv2.1 license. 

