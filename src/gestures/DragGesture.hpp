#pragma once
#include "Shared.hpp"

enum class DragGestureType {
    SWIPE,
    LONG_PRESS,
    EDGE_SWIPE,
    PINCH,
};

struct DragGestureEvent {
    uint32_t time;
    DragGestureType type;
    GestureDirection direction;
    uint32_t finger_count;

    GestureDirection edge_origin;

    std::string to_string() const;
};
