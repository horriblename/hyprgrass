#include "Shared.hpp"

std::string stringifyDirection(GestureDirection direction) {
    std::string bind;
    if (direction & GESTURE_DIRECTION_LEFT) {
        bind += 'l';
    }

    if (direction & GESTURE_DIRECTION_RIGHT) {
        bind += 'r';
    }

    if (direction & GESTURE_DIRECTION_UP) {
        bind += 'u';
    }

    if (direction & GESTURE_DIRECTION_DOWN) {
        bind += 'd';
    }

    return bind;
}

std::string stringifyPinchDirection(PinchDirection direction) {
    if (direction == PinchDirection::IN) {
        return "i";
    } else {
        return "o";
    }
}
