#pragma once
#include "Shared.hpp"
#include <string>

enum class CompletedGestureType {
    // Invalid Gesture
    SWIPE,
    EDGE_SWIPE,
    TAP,
    LONG_PRESS,
    // PINCH,
};

struct CompletedGesture {
    CompletedGestureType type;
    GestureDirection direction;
    int finger_count;

    // TODO turn this whole struct into a sum type?
    // edge swipe specific
    GestureDirection edge_origin;

    std::string to_string() const;
};
