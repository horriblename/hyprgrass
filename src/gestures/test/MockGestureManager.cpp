#include "MockGestureManager.hpp"
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
