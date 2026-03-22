#include "Gesture.hpp"
#include "Debouncer.hpp"
#include <memory>

void BacklightGesture::begin(const STrackpadGestureBegin& e) {
    g_pGlobalState->accumulated_delta += e.pinch->delta;
    g_pDebouncer->start();
}

void BacklightGesture::update(const STrackpadGestureUpdate& e) {
    g_pGlobalState->accumulated_delta += e.pinch->delta;
    g_pDebouncer->start();
}

void BacklightGesture::end(const STrackpadGestureEnd& e) {
    g_pDebouncer->disarm();
}

bool BacklightGesture::isDirectionSensitive() {
    return true;
}
