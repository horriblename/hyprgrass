#include "MockGestureManager.hpp"
#include "../src/Gestures.hpp"
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {
    auto emulateSwipeBegin = []() {
        std::cout << "emulate workspace swipe begin";
    };
    auto emulateSwipeEnd = []() { std::cout << "emulate workspace swipe end"; };

    addTouchGesture(newWorkspaceSwipeStartGesture(
        CONFIG_SENSITIVITY, emulateSwipeBegin, emulateSwipeEnd));
}
