# Configuration

## Configuration options

```
plugin {
 touch_gestures {
  # The default sensitivity is probably too low on tablet screens,
  # I recommend turning it up to 4.0
  sensitivity = 1.0

  # must be >= 3
  workspace_swipe_fingers = 3

  # switching workspaces by swiping from an edge, this is separate from workspace_swipe_fingers
  # and can be used at the same time
  # possible values: l, r, u, or d
  # to disable it set it to anything else
  workspace_swipe_edge = d

  # in milliseconds
  long_press_delay = 400

  # resize windows by long-pressing on window borders and gaps.
  # If general:resize_on_border is enabled, general:extend_border_grab_area is used for floating
  # windows
  resize_on_border_long_press = true

  # in pixels, the distance from the edge that is considered an edge
  edge_margin = 10

  # emulates touchpad swipes when swiping in a direction that does not trigger workspace swipe.
  # ONLY triggers when finger count is equal to workspace_swipe_fingers
  #
  # might be removed in the future in favor of event hooks
  emulate_touchpad_swipe = false

  experimental {
    # send proper cancel events to windows instead of hacky touch_up events,
    # NOT recommended as it crashed a few times, once it's stabilized I'll make it the default
    send_cancel = 0
  }
 }
}
```

### Other options

I also recommend that you adjust the settings for the built-in gesture to make
it easier to switch workspaces:

```
gestures {
  workspace_swipe = true
  workspace_swipe_cancel_ratio = 0.15
}
```

## Custom Commands

There are two ways to bind gesture events to some action.

1. use `hyprgrass-bind`. The syntax is like normal keybinds
2. use `hyprgrass-gesture`. The syntax is like `gesture`s (currently unstable,
   prefer the `hyprgrass-bindm` counterpart if possible)

You can also bind gesture events to dispatchers, using hyprgrass-bind keyword.
The syntax is like normal keybinds.

### `hyprgrass-bind`

#### Syntax

```
hyprgrass-bind = , <gesture_name>, <dispatcher>, <args>
```

where (skip to [examples](#examples) if this is confusing):

- `gesture_name` is one of:
  1. `swipe:<finger_count>:<direction>`
     - `finger_count`
     - `direction` is one of `l`, `r`, `u`, `d`, or `ld`, `rd`, `lu`, `ru` for
       diagonal directions.\
       (l, r, u, d stand for left, right, up, down)
  2. `tap:<finger_count>`
  3. `edge:<from_edge>:<direction>`
     - `<from_edge>` is from which edge to start from (l/r/u/d)
     - `<direction>` is in which direction to swipe (l/r/u/d/lu/ld/ru/rd)
  4. `longpress:<finger_count>`

#### Examples

```
plugin {
    touch_gestures {
        # swipe left from right edge
        hyprgrass-bind = , edge:r:l, workspace, +1

        # swipe up from bottom edge
        hyprgrass-bind = , edge:d:u, exec, firefox

        # swipe down from left edge
        hyprgrass-bind = , edge:l:d, exec, pactl set-sink-volume @DEFAULT_SINK@ -4%

        # swipe down with 4 fingers
        hyprgrass-bind = , swipe:4:d, killactive

        # swipe diagonally left and down with 3 fingers
        # l (or r) must come before d and u
        hyprgrass-bind = , swipe:3:ld, exec, foot

        # tap with 3 fingers
        hyprgrass-bind = , tap:3, exec, foot

        # longpress can trigger mouse binds:
        hyprgrass-bindm = , longpress:2, movewindow
        hyprgrass-bindm = , longpress:3, resizewindow
    }
}
```

### `hyprgrass-gesture`

`hyprgrass-gesture` supports the builtin actions of Hyprland's
[gestures](https://wiki.hypr.land/Configuring/Gestures/).

The `move` action is known to work poorly, use [bindm](#hyprgrass-bind) with
`movewindow` instead.

#### Syntax

```
hyprgrass-gesture = <gesture_type>, <gesture_arg>, <direction>, [modifiers, ...] <action> [, <options>...]
```

**Gesture type** is one of

- `swipe`
- `edge`
- `longpress`

**Gesture arg** is either:

- For `edge` events: The direction of the edge from which to begin, one of:
  `up`, `down`, `left`, `right`
- For other events: The number of fingers to trigger the action

**Direction** is one of:

- `swipe` -> any swipe
- `horizontal` -> horizontal swipe
- `vertical` -> vertical swipe
- `left`, `right`, `up`, `down` -> swipe directions

`longpress` events should leave this field empty.

You can add zero or more **modifiers** after the direction:

- `mod: MODMASK` to add a modifier key
- `scale: SCALE` to add scale the animation speed by a float

The following **actions** are currently supported (see
[Hyprland wiki](https://wiki.hypr.land/Configuring/Gestures/#available-gestures)
for details):

```
dispatcher
workspace
move
resize
special
close
fullscreen
float
```

#### Examples

```
hyprgrass-gesture = swipe, 3, down, close
hyprgrass-gesture = edge, 3, down, special
```

## Hyprgrass-pulse

see [](../examples/hyprgrass-pulse/README.md)
