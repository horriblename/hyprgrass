#include "MockGestureManager.hpp"
#include "../Gestures.hpp"
#include <cassert>
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {}

bool CMockGestureManager::handleCompletedGesture(const CompletedGesture& gev) {
    std::cout << "gesture triggered: " << gev.to_string() << "\n";
    this->triggered = true;
    return true;
}

bool CMockGestureManager::handleDragGesture(const DragGesture& gev) {
    std::cout << "drag started: " << gev.to_string() << "\n";
    return true;
}

void CMockGestureManager::handleCancelledGesture() {
    std::cout << "gesture cancelled\n";
    this->cancelled = true;
}

void CMockGestureManager::sendCancelEventsToWindows() {
    std::cout << "cancel touch on windows\n";
}
