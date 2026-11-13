The doctest suite exercises gesture recognition and touch-sequence routing.
Install doctest and the normal build dependencies, then run from the repository
root (with the `wf-touch` submodule initialized):

```sh
meson setup build -Dtests=enabled
meson compile -C build
meson test -C build --print-errorlogs
```

`TouchSequence.cpp` tests namespace matching, sequence ownership, overlapping
fingers, cancellation, device removal and slot reuse without a compositor.
It also routes a held/moving contact around the real gesture recognizer, then
checks that a subsequent normal long press still starts and ends a drag.
These tests do not exercise Hyprland's hit testing or real device signals.

## Manual compositor checks for layer exclusions

With `excluded_layer_namespaces = "osk, test-panel"` and existing one-finger
window-drag and border-resize bindings/settings:

- Hold Backspace longer than `long_press_delay`, then release. Repeat must
  continue while held and stop on release; no window drag/resize should start.
- Repeat on a second layer-shell app with a different listed namespace.
- Check popups and all four layer levels; a covered surface or a hole in its
  input region must not exclude the surface actually receiving the touch.
- Slide off an excluded surface, hide it, and reload the exclusion list while
  holding. The sequence must stay bypassed until every contact ends.
- Overlap two and three fingers, lifting the first finger before the others.
  Include a finger outside the panel and one on a second touch device.
- Cancel a contact or unplug its device during a hold. Remaining contacts must
  stay bypassed; after all contacts end, a new window gesture must work.
- Start a gesture on a normal window, then add a finger on an excluded panel:
  the gesture must retain its original route.
- Check normal one-finger dragging and border resizing before and after an
  excluded sequence. With an empty exclusion list, behavior must match the
  unmodified plugin. Check with `hold_touches` both enabled and disabled.
- Start a new sequence while the session is locked. It must use the existing
  lock-screen behavior, even if the underlying panel's namespace is listed.

Record compositor/plugin revisions and distinguish these hardware results from
the automated doctest checks.
