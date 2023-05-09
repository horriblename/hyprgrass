# Hyprland Touch Gestures

⚠️ WARNING this plugin is very alpha, expect breaking changes!

## Features/Roadmap

- [x] Workspace Swipe (currently only 3 finger allowed)
  - [ ] Configurable finger count
- [x] Custom commands
- [x] Swipe From Edge
- [ ] _n_-Finger swipe

## Configuration

### Configuration options:

```
plugin {
  touch_gestures {
    # default sensitivity is probably too low on tablet screens,
    # I recommend turning it up to 4.0
    sensitivity = 1.0
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
#   bind = , <gesture_name>, <dispatcher>, <args>
```

where:

- `gesture_name` is `[edge|swipe]<finger_count><direction>`
  - currently **only `edge` is implemented**
  - `finger_cnumberount` is the number of fingers to use
  - `direction` is one of `l`, `r`, `u`, `d`
  - e.g. `edge1l` means swipe from the _right edge_

#### Examples

```
# swipe left from right edge
bind = , edge1l, workspace, +1

# swipe up from bottom edge
bind = , edge1u, exec, firefox
```
