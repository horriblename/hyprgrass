#pragma once
#include "Shared.hpp"

enum class DragGestureType {
    SWIPE,
    LONG_PRESS,
    EDGE_SWIPE,
};

struct DragGesture {
    DragGestureType type;
    GestureDirection direction;
    int finger_count;

    GestureDirection edge_origin;

    std::string to_string() const;
};
