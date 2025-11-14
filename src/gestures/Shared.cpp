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

    if (direction & GESTURE_DIRECTION_IN) {
        bind += 'i';
    }

    if (direction & GESTURE_DIRECTION_OUT) {
        bind += 'o';
    }

    return bind;
}
