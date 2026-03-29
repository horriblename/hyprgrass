#include "Gesture.hpp"
#include "Debouncer.hpp"

static bool isVert(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_VERTICAL:
        case TRACKPAD_GESTURE_DIR_UP:
        case TRACKPAD_GESTURE_DIR_DOWN:
            return true;
        default:
            return false;
    }
}

void PulseGesture::begin(const STrackpadGestureBegin& e) {
    const bool vert = isVert(e.direction);
    g_pGlobalState->accumulated_delta += vert ? -e.swipe->delta.y : e.swipe->delta.x;
    g_pDebouncer->start();
}

void PulseGesture::update(const STrackpadGestureUpdate& e) {
    const bool vert = isVert(e.direction);
    g_pGlobalState->accumulated_delta += vert ? -e.swipe->delta.y : e.swipe->delta.x;
    g_pDebouncer->start();
}

void PulseGesture::end(const STrackpadGestureEnd& e) {
    g_pDebouncer->disarm();
}

bool PulseGesture::isDirectionSensitive() {
    return true;
}
