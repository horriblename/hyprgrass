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

- `gesture_name` is `[edge|swipe]<finger_count><direction>`
  - `edge` for edge swipe, `swipe` for multi-finger swipe
  - `finger_cnumberount` is the number of fingers to use, this is always 1 for `edge`
  - `direction` is one of `l`, `r`, `u`, `d`, or `ld`, `rd`, `lu`, `ru` for diagonal directions.
  - e.g. `edge1l` means swipe left from the _right edge_

> :warning: `<gesture_name>` with misspellings will be silently ignored.

#### Examples

```
# swipe left from right edge
bind = , edge1l, workspace, +1

# swipe up from bottom edge
bind = , edge1u, exec, firefox

# swipe down with 4 fingers
bind = , swipe4d, killactive

# swipe diagonally leftwards and downwards with 3 fingers
# l (or r) must come before d and u
bind = , swipe3ld, exec, foot
```
