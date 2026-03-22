# hyprgrass-backlight

Control brightness of backlight via hyprgrass edge gestures.

Haphazardly put together using stolen code from
[Waybar](https://github.com/Alexays/Waybar), licensed under MIT. All credits go
to Alexays.

## Config

Use the `backlight-gesture` keyword. Flags are the same as Hyprland `gesture`.
Only `backlight` action is available.

```hyprlang
backlight-gesture = 3, vertical, backlight
```

[`gestures:workspace_swipe_distance`](wiki.hypr.land/Configuring/Variables/#gestures)
is used to determine the distance in px to change the backlight from 0 to 100
