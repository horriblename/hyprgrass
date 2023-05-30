# Hyprland Touch Gestures

⚠️ WARNING this plugin is very alpha, expect breaking changes!

## Features/Roadmap

- [x] Workspace Swipe
- [x] Custom commands
- [x] Swipe From Edge
- [x] Multi-finger swipe

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
