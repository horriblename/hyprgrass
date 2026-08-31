# Configuration

> [!NOTE]
>
> If you're migrating from Hyprlang, be sure to read the
> [Lua migration guide](./lua_migration.md) for relevant breaking changes

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

```lua
hl.plugin.hyprgrass.bind {
    pattern : Pattern -- see below
    action : function
    -- optional flags
    locked : boolean?,
    mouse : boolean?, -- currently only works with longpress
    non_consuming : boolean?, 
}
```

`pattern` is almost the same as the `pattern` in
[`hyprgrass.gesture`](#hyrgrassgesture-syntax), with two differences:

1. Only a single specific `direction` (left/right/up/down) is accepted
2. 2 additional kinds:
   - `{kind="tap", fingers = n}`
   - `{kind="pinch", fingers = n, direction="pinchin" | "pinchout"}`

#### hyprgrass-bind Examples

```lua
hl.plugin.hyprgrass.bind {
    pattern = {kind = "edge", origin = "d", direction = "l"},
    action = hl.dsp.focus({workspace = "+1"}),
}

hl.plugin.hyprgrass.bind {
    pattern = {kind = "edge", origin = "d", direction = "u", fingers = 2},
    action = hl.dsp.exec_cmd("firefox"),
}

hl.plugin.hyprgrass.bind {
    pattern = {kind = "edge", origin = "d", direction = "u"},
    mod = "ALT+SHIFT",
    action = hl.dsp.exec_cmd("firefox"),
}

-- longpress can trigger mouse binds:
hl.plugin.hyprgrass.bind {
    pattern = {kind = "longpress", fingers = 3}
    action = hl.dsp.window.drag(),
    mouse = true,
}

hl.plugin.hyprgrass.bind {
    pattern = {kind = "tap", fingers = 3}
    action = hl.dsp.window.float(),
}

hl.plugin.hyprgrass.bind {
    pattern = {kind = "pinch", fingers = 3, direction = "pinchin"}
    action = hl.dsp.exec_cmd("foot"),
}
```

### `hyprgrass-gesture`

`hyprgrass-gesture` supports the builtin actions of Hyprland's
[gestures](https://wiki.hypr.land/Configuring/Advanced-and-Cool/Gestures/).

#### `hyrgrass.gesture` Syntax

```lua
hl.plugin.hyprgrass.gesture {
    pattern = {kind = "swipe", fingers = 3, direction = "down"},
    action = "close",
}
```

`pattern` is one of:

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
    pattern = {kind = "longpress", fingers = 3}
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
<live lua gestures>
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

#### Live Lua Gestures

For live gestures, i.e. ones that react to the gesture state, pass a table
instead of a lambda, which has start, update, and finish methods.

The start and update methods are passed a table with the following fields:

| Field    | Type    | Description                                                                                                                                                           |
| -------- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| type     | string  | One of: `swipe`, `edge`, `longpress`                                                                                                                                  |
| time_ms  | integer | The timestamp at which the event occurred                                                                                                                             |
| fingers  | integer | Number of fingers                                                                                                                                                     |
| delta.x  | float   | Horizontal motion relative to the last update. Right motion is positive, left is negative                                                                             |
| delta.y  | float   | Vertical motion relative to the last update. Downwards motion is positive, upwards is negative                                                                        |
| scale    | float   | The change in size of the finger arrangement, relative to the start of the gesture. Spread is positive, pinch is negative. Nil if the gesture type is not pinch       |
| rotation | float   | The change in angle of the finger arrangement, relative to the last update. Clockwise is positive, counterclockwise is negative. Nil if the gesture type is not pinch |

The start method additional has:

- `pos: {x, y : float}`: the center point of the current positions of your
  fingers, in pixels
- `monitor: {width, height: float}`: size of the monitor where the gesture is
  started on, in pixels

The finish method is passed a table with the following fields:

| Field                                                                          | Type    | Description type                                                         | string | Either swipe or pinch time_ms | integer | The |
| ------------------------------------------------------------------------------ | ------- | ------------------------------------------------------------------------ | ------ | ----------------------------- | ------- | --- |
| timestamp at which the even occurred, measured from when the system was booted |         |                                                                          |        |                               |         |     |
| cancelled                                                                      | boolean | True if the gesture was ended abnormally by the backend. False otherwise |        |                               |         |     |

Example:

```lua
hl.plugin.hyprgrass.gesture {
    pattern = {...},
    action = {
        start = function()
        end,
    },
}
```

##### Live Gesture Extras

There are a couple of extra live gestures you can install/copy separately:

- `extras/wpctl.lua`: Control volume with gestures, using `wpctl`
- `extras/brightnessctl.lua`: Control brightness with gestures, using
  `brightnessctl`

To use them, copy the corresponding file (I'm using wpctl.lua as example below):

1. Copy `extras/wpctl.lua` to
   `~/.config/hypr/plugins/hyprgrass/extras/wpctl.lua`.

   > [!NOTE]
   > You can use `git` or whatever tool you wish to manage Lua plugins, just
   > make sure to adjust `package.path` below to match your own

2. Add this to your `hyprland.lua`:

   ```lua
   -- replace USER with your username
   package.path = package.path .. "/home/USER/.config/hypr/?.lua;/home/USER/.config/hypr/?/init.lua"
   if hl.plugin.hyprgrass then
       local hg_wpctl = require("plugins.hyprgrass.extras.wpctl")
       hl.plugin.hyprgrass.gesture({
           pattern = {kind="edge", origin="right", direction="vertical"}
           action = hg_wpctl.volume_action({
               direction = "vertical",
               flip = true,
           })
       })
   end
   ```

See the corresponding Lua files for available arguments.

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
    pattern = {kind = "edge", origin = "up", direction = "down"},
    action = "emulate_touchpad",
    emulate_fingers = 4,
    emulate_direction = "down",
}

hl.plugin.gesture {
    pattern = {kind = "swipe", fingers = 3, direction = "down"},
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
    pattern = {kind = "swipe", fingers = 3, direction = "down"},
    action = "close",
}

-- Swipe from upper edge downwards
hl.plugin.hyprgrass.gesture {
    pattern = {kind = "swipe", origin = "up", direction = "down"},
    action = "special",
}

-- Workspace does not work with the "swipe" direction,
-- make sure to put in an accepted direction even for longpress
```

```hyprlang
hyprgrass-gesture = longpress, 3, horizontal, workspace
```
