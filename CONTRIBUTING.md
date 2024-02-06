# Project Structure

## Intro

The two most important files/classes are `IGestureManger` in `src/gestures/Gestures.hpp` and
`GestureManager` in `src/GestureManager.hpp`

- `IGestureManager` processes touch events (`onTouchDown`) and then emits events when a gesture is triggered/cancelled.
- `GestureManager` implements the event handlers defined in `IGestureManger`.

**Actions**: `wf::touch::action_t` describe a sequence of touch events and are chained together to implement gestures. Custom actions are defined in `src/gestures/Actions.cpp`

## Code Guidelines

1. Everything in `src/gestures` should be easily testable - do NOT put Hyprland specific code in this
   directory.

# Commit guidelines

1. There are no strict commit message guidelines - I loosely follow [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/). Follow them if possible, it's fine if you don't.

# A note on wf-touch

TODO

<!-- `wf-touch` - the library we are using for the fancy gesture detection math - can be hard to work with/understand. If you want to add a new gesture type (e.g. pinch, swipe, rotate etc.), just leave a feature request, but if you're dedicated enough to do it yourself, you should be familiar with how `wf::touch::action_t` work  -->
