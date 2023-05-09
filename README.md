# Hyprland Touch Gestures

⚠️ WARNING this plugin is very alpha, expect breaking changes!

## Features/Roadmap

- [x] Workspace Swipe (currently only 3 finger allowed)
  - [ ] Configurable finger count
- [ ] Custom commands
- [ ] Swipe From Edge
- [ ] _n_-Finger swipe

## Configuration

```
plugin {
  touch_gestures {
    workspace_swipe_fingers = 3
    # default sensitivity is probably too low on tablet screens,
    # I recommend turning it up to 5.0
    sensitivity = 1.0
  }
}
```

I also recommend you adjust the settings of the builtin gesture to make it easier to switch workspaces:

```
gestures {
  workspace_swipe = true
  workspace_swipe_cancel_ratio = 0.15
}
```
