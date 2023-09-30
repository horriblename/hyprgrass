# Hyprland Touch Gestures

⚠️ WARNING there may still be some bugs that leaves your touch device unusable until you unload the plugin/close hyprland (https://github.com/horriblename/hyprgrass/issues/27), keep a keyboard in hand the first time you try this.

this plugin is very alpha, expect breaking changes!  

Please open an issue if you encounter any bugs. Feel free to make a feature request if you have a suggestion.

## Features/Roadmap

- [x] Workspace Swipe
- [x] Custom commands
- [x] Swipe From Edge
- [x] Multi-finger swipe

## Installation

### Dependencies

Asides from hyprland (duh), this plugin has the following dependencies:

```
glm
```

### Install via hyprload

The easiest way to use this plugin is by using [Hyprload](https://github.com/Duckonaut/hyprload) (a plugin manager).

1. install all [dependencies](#dependencies)
2. install hyprload by following the instructions
   [here](https://github.com/Duckonaut/hyprload#Installing)
3. put this in `~/.config/hypr/hyprload.toml`:
   ```
   plugins = [
       "horriblename/hyprgrass",
   ]
   ```
4. run this command:

   ```bash
   # installs plugins
   hyprctl dispatch hyprload install

   # load plugins
   hyprctl dispatch hyprload load
   ```

### Manual Compilation

```bash
meson setup build
ninja -C build
```

### Install via nix

flakes are highly recommended (because I don't know how to do anything without it)

put this in your `flake.nix`

```nix
{
   inputs = {
      # ...
      hyprland.url = "github:hyprwm/Hyprland";
      hyprgrass = {
         url = "github:horriblename/hyprgrass";
         inputs.hyprland.follows = "hyprland"; # IMPORTANT
      };
   };
}
```

then, inside your home-manager module:

```nix
wayland.windowManager.hyprland = {
   plugins = [
      inputs.hyprgrass.packages.${pkgs.system}.default
   ];
};
```

## Configuration

### Configuration options:

```
plugin {
  touch_gestures {
    # default sensitivity is probably too low on tablet screens,
    # I recommend turning it up to 4.0
    sensitivity = 1.0
    workspace_swipe_fingers = 3
  }
}
```

#### Other options

I also recommend you adjust the settings of the builtin gesture to make it easier to switch workspaces:

```
gestures {
  workspace_swipe = true
  workspace_swipe_cancel_ratio = 0.15
}
```

### Custom Commands

You can also bind gesture events to dispatchers, it currently uses the builtin bind keyword like
keybinds. This will likely change in the future.

#### Syntax

```
bind = , <gesture_name>, <dispatcher>, <args>
```

where (skip to [examples](#examples) if this is confusing):

- `gesture_name` is one of:
  1. `swipe:<finger_count>:<direction>`
     - `direction` is one of `l`, `r`, `u`, `d`, or `ld`, `rd`, `lu`, `ru` for diagonal directions.  
       (l, r, u, d stands for left, right, up, down)
  2. `edge:<from_edge>:<direction>`
     - `<from_edge>` is from which edge to start (l/r/u/d)
     - `<direction>` is in which direction to swipe (l/r/u/d/lu/ld/ru/rd)

> :warning: `<gesture_name>` with misspellings will be silently ignored.

#### Examples

```
# swipe left from right edge
bind = , edge:r:l, workspace, +1

# swipe up from bottom edge
bind = , edge:d:u, exec, firefox

# swipe down from left edge
bind = , edge:l:d, exec, pactl set-sink-volume @DEFAULT_SINK@ -4%

# swipe down with 4 fingers
bind = , swipe:4:d, killactive

# swipe diagonally leftwards and downwards with 3 fingers
# l (or r) must come before d and u
bind = , swipe:3:ld, exec, foot
```

# Acknowledgements

Special thanks to wayfire for the awesome [wf-touch](https://github.com/WayfireWM/wf-touch) library!
