# Lua migration guide

## Breaking changes

A few things to keep in mind for your migration to Lua:

1. The plugin namespace changed from `touch_gestures` to `hyprgrass`:

   ```lua
   -- instead of {plugin = { touch_gestures = {...} }}
   hl.config({plugin = {hyprgrass = {...}}})
   ```

2. I'm considering deprecating diagonal gestures in `hyprgrass.bind`, for the
   time being you can still use them with a string pattern, just know I might
   remove them at some point:

   ```lua
   hl.plugin.hyprgrass.bind({
       pattern = "swipe:3:ld",
       action = hl.dsp.focus({workspace = "+1"}),
   })
   ```

3. Config options `workspace_swipe_fingers` and `workspace_swipe_edge` are
   removed in favor of `hyprgrass.gesture`:

   ```lua
   -- workspace_swipe_fingers
   hl.plugin.hyprgrass.gesture({
       pattern = {kind = "swipe", fingers = 3, direction = "horizontal"},
       action = "workspace"
   })

   -- workspace_swipe_edge
   hl.plugin.hyprgrass.gesture({
       pattern = {kind = "edge", origin = "down", direction = "horizontal"},
       action = "workspace",
   })
   ```

4. Removed deprecated options.

## Config options

```lua
hl.config({
    plugin = {
        hyprgrass = {
            sensitivity = 4.0,
            -- ...
        },
    },
})
```

## hyprgrass-bind

Use the `hl.plugin.hyprgrass.bind` function:

```lua
-- hyprgrass-bind = , swipe:3:d, exec, firefox
hl.plugin.hyprgrass.bind {
    -- the old "swipe:3:d" is still usable if you want
    pattern = {kind = "swipe", fingers = 3, direction = "down"},
    -- action can also be a lua function
    action = hl.dsp.exec_cmd("firefox"),

    -- optional args:
    mod = "SHIFT + ALT",
    locked = true,
    mouse = true, -- old 'm' flag: hyprgrass-bindm
}

-- more patterns:
-- edge:u:d    => {kind="edge", origin="up", direction="down"}
-- longpress:3 => {kind="longpress", fingers=3}
-- tap:3       => {kind="tap", fingers=3}
-- pinch:3:i   => {kind="pinch", fingers=3, direction="pinchin"}
```

## hyprgrass-gesture

Use the `hl.plugin.hyprgrass.gesture` function:

```lua
-- hyprgrass-gesture = swipe, 3, down, workspace
hl.plugin.hyprgrass.gesture {
    pattern = {kind = "swipe", fingers = 3, direction = "horizontal"},
    action = "workspace",

    -- optional args:
    mod = "SHIFT + ALT",
    scale = 2.0,
}

-- more patterns:
-- edge, up, horizontal => {kind="edge", origin="up", direction="horizontal"}
-- longpress, 3, left   => {kind="longpress", fingers = 3, direction="left"}
-- pinch, 3, pinch      => {kind="pinch", fingers=3, direction="pinch"}
```

Take a look at the [docs](./configuration.md) for more details.
