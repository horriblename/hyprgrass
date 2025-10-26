# hyprgrass-backlight

Control brightness of backlight via hyprgrass edge gestures.

Haphazardly put together using stolen code from [Waybar](https://github.com/Alexays/Waybar),
licensed under MIT. All credits go to Alexays.

## Config

```
plugin {
    hyprgrass-backlight {
        # Along which edge to trigger the volume changer
        # Slide along the edge to adjust volume
        # One of: l, r, u, d
        edge = l
    }
}
```
