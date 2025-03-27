# Hyprgrass

Gestures for your touch screen

> [!WARNING]
> Even though hyprgrass is mostly stable now, there used to be some bugs that render your touch device unusable until you unload the plugin/close Hyprland (https://github.com/horriblename/hyprgrass/issues/27), keep a keyboard in hand the first time you try this. This plugin is still in alpha, expect breaking changes!

Please open an issue if you find any bugs. Feel free to make a feature request if you have a suggestion.

## Features/Roadmap

- [x] Workspace Swipe
- [x] Custom commands
- [x] Swipe From Edge
- [x] Multi-finger swipe

## Installation

### Dependencies

Besides Hyprland (duh), this plugin has the following dependencies:

```
glm

# build dependencies
meson
ninja

# extra dependencies for hyprgrass-pulse
libpulseaudio
```

### Install via hyprpm

First, install all [dependencies](#dependencies). Then, run these commands:

```bash
hyprpm add https://github.com/horriblename/hyprgrass
hyprpm enable hyprgrass

# optional integration with pulse-audio, see examples/hyprgrass-pulse/README.md
hyprpm enable hyprgrass-pulse
```

You can add `exec-once = hyprpm reload -n` to your hyprland config to have plugins loaded at
startup. -n will make hyprpm send a notification if anything goes wrong (e.g. update needed)

see [hyprland wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#hyprpm) for more info

### Install via Hyprload

(hyprload is deprecated, please use hyprpm instead)

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
   # install the plugins
   hyprctl dispatch hyprload install

   # load plugins
   hyprctl dispatch hyprload load
   ```

### Manual compilation

```bash
meson setup build
ninja -C build
```

On the `meson setup` step you can pass these options:
- `-Dhyprgrass-pulse=true` to enable building hyprgrass-pulse

### Install via nix

Flakes are highly recommended (because I don't know how to do anything without them)

Put this in your `flake.nix` file:

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

and in your home-manager module:

```nix
wayland.windowManager.hyprland = {
   plugins = [
      inputs.hyprgrass.packages.${pkgs.system}.default

      # optional integration with pulse-audio, see examples/hyprgrass-pulse/README.md
      inputs.hyprgrass.packages.${pkgs.system}.hyprgrass-pulse
   ];
};
```

#### Overlay

(untested, let me know if it works)

There is an overlay provided, which should work well if you're using hyprland from nixpkgs.

Add the hyprgrass overlay to your pkgs:

```nix
pkgs = import inputs.nixpkgs {
    system = "...";
    overlays = [inputs.hyprgrass.overlays.default];
};
```

Hyprgrass is now available under `pkgs.hyprlandPlugins`

```nix
wayland.windowManager.hyprland = {
    plugins = [
        pkgs.hyprlandPlugins.hyprgrass
    ];
};
```

## Configuration

### Configuration options:

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

#### Other options

I also recommend that you adjust the settings for the built-in gesture to make it easier to switch workspaces:

```
gestures {
  workspace_swipe = true
  workspace_swipe_cancel_ratio = 0.15
}
```

### Custom Commands

You can also bind gesture events to dispatchers, using hyprgrass-bind keyword.
The syntax is like normal keybinds.

### Hyprgrass-pulse

see [./examples/hyprgrass-pulse/README.md](./examples/hyprgrass-pulse/README.md)

#### Syntax

```
hyprgrass-bind = , <gesture_name>, <dispatcher>, <args>
```

where (skip to [examples](#examples) if this is confusing):

- `gesture_name` is one of:
  1. `swipe:<finger_count>:<direction>`
     - `finger_count`
     - `direction` is one of `l`, `r`, `u`, `d`, or `ld`, `rd`, `lu`, `ru` for diagonal directions.  
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

# Other touch screen goodies

Touch screen related tools I liked.

On-screen keyboards:

- [squeekboard](https://gitlab.gnome.org/World/Phosh/squeekboard): has auto show/hide but doesn't
  work well with IME (fcitx/IBus etc.)
- [wvkbd](https://github.com/jjsullivan5196/wvkbd): relatively simple keyboard but still has most
  important features.
- [fcitx-virtual-keyboard-adapter](https://github.com/horriblename/fcitx-virtualkeyboard-adapter):
  NOT a keyboard but an fcitx addon that auto show/hides any on-screen keyboard.

Miscellanaeious:

- [iio-hyprland](https://github.com/JeanSchoeller/iio-hyprland/): auto screen rotation
- [Hyprspace](https://github.com/KZDKM/Hyprspace): an overview plugin for hyprland
- [nwg-drawer](https://github.com/nwg-piotr/nwg-drawer): app drawer. Surprisingly, there's not a
  lot of those 

# Acknowledgements

Special thanks to wayfire for the awesome [wf-touch](https://github.com/WayfireWM/wf-touch) library!
