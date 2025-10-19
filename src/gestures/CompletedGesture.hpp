#pragma once
#include "Shared.hpp"
#include <string>

enum class CompletedGestureType {
    // Invalid Gesture
    SWIPE,
    EDGE_SWIPE,
    TAP,
    LONG_PRESS,
    PINCH,
};

struct CompletedGestureEvent {
    CompletedGestureType type;
    GestureDirection direction;
    uint32_t finger_count;

    // TODO turn this whole struct into a sum type?
    // edge swipe specific
    GestureDirection edge_origin;
    PinchDirection pinch_direction;

    std::string to_string() const;
};
