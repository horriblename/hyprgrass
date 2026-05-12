# hyprgrass-pulse

Control pulse audio volume via hyprgrass touchpad gestures.

Haphazardly put together using stolen code from
[Waybar](https://github.com/Alexays/Waybar), licensed under MIT. All credits go
to Alexays.

## Config

Use the `pulse-gesture` keyword. Flags are the same as Hyprland `gesture`. Only
`volume` action is available.

```hyprlang
# binds to a touchpad gesture
pulse-gesture = 3, vertical, volume

# also bind to a hyprgrass gesture
hyprgrass-gesture = 3, vertical, emulate_touchpad, 3, vertical
```

[`gestures:workspace_swipe_distance`](wiki.hypr.land/Configuring/Variables/#gestures)
is used to determine the distance in px to change the volume from 0 to 100
