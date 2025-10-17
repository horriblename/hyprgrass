# Hyprgrass

Gestures for your touch screen

> [!WARNING]
> Even though hyprgrass is mostly stable now, there used to be some bugs that
> render your touch device unusable until you unload the plugin/close Hyprland
> see [issue #27](https://github.com/horriblename/hyprgrass/issues/27), keep a
> keyboard in hand the first time you try this. This plugin is still in alpha,
> expect breaking changes!

Please open an issue if you find any bugs. Feel free to make a feature request
if you have a suggestion.

## Features/Roadmap

- [x] Workspace Swipe
- [x] Custom commands
- [x] Swipe From Edge
- [x] Multi-finger swipe

## Installation

### Dependencies

Besides Hyprland (duh), this plugin has the following dependencies:

```bash
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

You can add `exec-once = hyprpm reload -n` to your hyprland config to have
plugins loaded at startup. -n will make hyprpm send a notification if anything
goes wrong (e.g. update needed)

see [hyprland wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#hyprpm) for
more info

### Install via Hyprload

(hyprload is deprecated, please use hyprpm instead)

1. install all [dependencies](#dependencies)
2. install hyprload by following the instructions
   [here](https://github.com/Duckonaut/hyprload#Installing)
3. put this in `~/.config/hypr/hyprload.toml`:

   ```toml
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

Flakes are highly recommended (because I don't know how to do anything without
them)

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

## Configuration

See [docs](/docs/configuration.md)

## Other touch screen goodies

Touch screen related tools I liked.

On-screen keyboards:

- [squeekboard](https://gitlab.gnome.org/World/Phosh/squeekboard): has auto
  show/hide but doesn't work well with IME (fcitx/IBus etc.)
- [wvkbd](https://github.com/jjsullivan5196/wvkbd): relatively simple keyboard
  but still has most important features.
- [fcitx-virtual-keyboard-adapter](https://github.com/horriblename/fcitx-virtualkeyboard-adapter):
  NOT a keyboard but an fcitx addon that auto show/hides any on-screen keyboard.

Miscellanaeious:

- [iio-hyprland](https://github.com/JeanSchoeller/iio-hyprland/): auto screen
  rotation
- [Hyprspace](https://github.com/KZDKM/Hyprspace): an overview plugin for
  hyprland
- [nwg-drawer](https://github.com/nwg-piotr/nwg-drawer): app drawer.
  Surprisingly, there's not a lot of those

## Acknowledgements

Special thanks to wayfire for the awesome
[wf-touch](https://github.com/WayfireWM/wf-touch) library!
