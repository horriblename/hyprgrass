# Configuration

## Options

```lua
hl.config({
    plugin = {
        hyprgrass = {
            -- The default sensitivity is probably too low on tablet screens,
            -- I recommend turning it up to 4.0
            sensitivity = 1.0,

            -- in milliseconds
            long_press_delay = 400,

            -- resize windows by long-pressing on window borders and gaps.
            -- If general:resize_on_border is enabled, general:extend_border_grab_area is
            -- used for floating windows
            resize_on_border_long_press = true,

            -- in pixels, the distance from the edge that is considered an edge
            edge_margin = 10,
        }
    }
})
```

### Other options

I also recommend that you adjust the settings for the built-in gesture to make
it easier to switch workspaces:

```lua
hl.config({
  gestures = {
    workspace_swipe_touch = true,
    workspace_swipe_cancel_ratio = 0.15,
  },
})
```

## Custom Commands

There are two ways to bind gesture events to some action.

1. `hl.plugin.hyprgrass.bind` - analogous to `hl.bind`, you can bind a gesture
   to a dispatcher, including mouse dispatchers (with `{mouse = true}`)
2. `hl.plugin.hyprgrass.gesture` - analogous to `hl.gesture`, works with actions
   that can be triggered by touchpad gestures

### `hyprgrass-bind`

TBD

#### hyprgrass-bind Examples

```lua
hl.plugin.hyprgrass.bind {
    gesture = "edge:d:l",
    action = hl.dsp.focus({workspace = "+1"}),
}

hl.plugin.hyprgrass.bind {
    gesture = "edge:d:u",
    mod = "ALT+SHIFT",
    action = hl.dsp.exec_cmd("firefox"),
}

-- longpress can trigger mouse binds:
hl.plugin.hyprgrass.bind {
    gesture = "longpress:3",
    action = hl.dsp.window.drag(),
    mouse = true,
}

hl.plugin.hyprgrass.bind {
    gesture = "tap:3",
    action = hl.dsp.window.float(),
}

hl.plugin.hyprgrass.bind {
    gesture = "pinch:3:i",
    action = hl.dsp.exec_cmd("foot"),
}
```

### `hyprgrass-gesture`

`hyprgrass-gesture` supports the builtin actions of Hyprland's
[gestures](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Gestures/).

#### hyrgrass-gesture Syntax

```hyprlang
hyprgrass-gesture = <gesture_type>, <gesture_arg>, <direction>, [modifiers, ...] <action> [, <options>...]
```

```lua
hl.plugin.hyprgrass.gesture {
    gesture = {kind = "swipe", fingers = 3, direction = "down"},
    action = "close",
}
```

`gesture` is one of:

```lua
{ kind = "swipe", fingers = ..., direction = ... }
{ kind = "edge", origin = ..., direction = ... }
{ kind = "longpress", fingers = ... }
```

`direction` is one of:

| direction                             | Description      |
| ------------------------------------- | ---------------- |
| `"swipe"`                             | any swipe        |
| `"horizontal"`                        | horizontal swipe |
| `"vertical"`                          | vertical swipe   |
| `"left"`, `"right"`, `"up"`, `"down"` | swipe directions |

> [!NOTE]
> Some actions only accept certain directions. E.g. `workspace` does not work
> with `swipe`. `longpress` events should use a valid direction in this case.

The `origin` field for `kind = "edge"` is one of: left, right, up, down

**Modifiers** can be added to the gesture:

- `mod: MODMASK` to add a modifier key
- `scale: SCALE` to add scale the animation speed by a float

```lua
hl.plugin.hyprgrass.gesture {
    gesture = {kind = "longpress", fingers = 3}
    mod = "ALT + SHIFT",
    scale = 2.5,
}
```

The following **actions** are currently supported (see
[Hyprland wiki](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Gestures/#Actions)
for details):

```text
# inherited from Hyprland
<lua function>
workspace
move
resize
special
close
fullscreen
float

# exclusive for Hyprgrass, see below
emulate_touchpad
```

#### `emulate_touchpad` action

This action emulates touchpad gestures. It takes two additional arguements:

```lua
emulate_fingers, emulate_direction
```

Both arguments are as described in
[Hyprland wiki](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Gestures/#Directions)
Example:

```lua
hl.plugin.gesture {
    gesture = {kind = "edge", origin = "up", direction = "down"},
    action = "emulate_touchpad",
    emulate_fingers = 4,
    emulate_direction = "down",
}

hl.plugin.gesture {
    gesture = {kind = "swipe", fingers = 3, direction = "down"},
    action = "emulate_touchpad",
    emulate_fingers = 3,
    emulate_direction = "down",
}
```

```hyprlang
hyprgrass-gesture = swipe, 3, down, emulate_touchpad, 3, down
```

#### hyprgrass-gesture Examples

```lua
hl.plugin.hyprgrass.gesture {
    gesture = {kind = "swipe", fingers = 3, direction = "down"},
    action = "close",
}

-- Swipe from upper edge downwards
hl.plugin.hyprgrass.gesture {
    gesture = {kind = "swipe", origin = "up", direction = "down"},
    action = "special",
}

-- Workspace does not work with the "swipe" direction,
-- make sure to put in an accepted direction even for longpress
```

```hyprlang
hyprgrass-gesture = longpress, 3, horizontal, workspace
```

## Hyprgrass-pulse

see [](../examples/hyprgrass-pulse/README.md)
