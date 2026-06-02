# Lua migration guide

A few things to keep in mind for your migration to Lua:

1. The plugin namespace changed from `touch_gestures` to `hyprgrass`:

   ```lua
   hl.config({plugin = {hyprgrass = {...}}})
   ```

2. Config options `workspace_swipe_fingers` and `workspace_swipe_edge` are
   removed in favor of `hyprgrass.gesture`:

   ```lua
   -- workspace_swipe_fingers
   hl.plugin.hyprgrass.gesture({
       gesture = {kind = "swipe", fingers = 3, direction = "horizontal"},
       action = "workspace"
   })

   -- workspace_swipe_edge
   hl.plugin.hyprgrass.gesture({
       gesture = {kind = "edge", origin = "down", direction = "horizontal"},
       action = "workspace",
   })
   ```

3. Removed deprecated options.

Take a look at the [docs](./configuration.md), this is just to warn you of some
technically breaking changes.
