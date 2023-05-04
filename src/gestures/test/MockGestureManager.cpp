#include "MockGestureManager.hpp"
#include "../Gestures.hpp"
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {}

void CMockGestureManager::addWorkspaceSwipeBeginGesture() {
    auto emulateSwipeBegin = [this]() {
        std::cout << "emulate workspace swipe begin\n";
        this->triggered = true;
    };
    auto emulateSwipeEnd = [this]() {
        std::cout << "emulate workspace swipe end\n";
        this->cancelled = true;
    };

    addTouchGesture(newWorkspaceSwipeStartGesture(
        CONFIG_SENSITIVITY, 3, emulateSwipeBegin, emulateSwipeEnd));
}

void CMockGestureManager::addEdgeSwipeGesture() {
    auto completed_cb = [this](CMultiAction*) {
        std::cout << "edge swipe triggered\n";
        this->triggered = true;
void CMockGestureManager::handleGesture(const TouchGesture& gev) {
    std::cout << "gesture triggered: " << gev.to_string() << "\n";
    this->triggered = true;
}

void CMockGestureManager::handleCancelledGesture() {
    std::cout << "gesture cancelled\n";
    this->cancelled = true;
}

    };
    auto cancelled_cb = [this](CMultiAction*) {
        std::cout << "edge swipe cancelled\n";
        this->cancelled = true;
    };

    addTouchGesture(
        newEdgeSwipeGesture(CONFIG_SENSITIVITY, completed_cb, cancelled_cb));
}
