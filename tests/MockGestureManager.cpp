#include "MockGestureManager.hpp"
#include "../src/Gestures.hpp"
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {}

void CMockGestureManager::addWorkspaceSwipeBeginGesture() {
    auto emulateSwipeBegin = [this]() {
        std::cout << "emulate workspace swipe begin\n";
        this->workspaceSwipeTriggered = true;
    };
    auto emulateSwipeEnd = [this]() {
        std::cout << "emulate workspace swipe end\n";
        this->workspaceSwipeCancelled = true;
    };

    addTouchGesture(newWorkspaceSwipeStartGesture(
        CONFIG_SENSITIVITY, emulateSwipeBegin, emulateSwipeEnd));
}
