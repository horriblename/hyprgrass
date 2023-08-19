#include "MockGestureManager.hpp"
#include "../Gestures.hpp"
#include <cassert>
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {}

bool CMockGestureManager::handleGesture(const CompletedGesture& gev) {
    std::cout << "gesture triggered: " << gev.to_string() << "\n";
    this->triggered = this->triggered || gev.type != GESTURE_TYPE_SWIPE_HOLD;
    return true;
}

void CMockGestureManager::handleCancelledGesture() {
    std::cout << "gesture cancelled\n";
    this->cancelled = true;
}

void CMockGestureManager::sendCancelEventsToWindows() {
    std::cout << "cancel touch on windows\n";
}
