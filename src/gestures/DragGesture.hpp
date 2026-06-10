#pragma once
#include "CompletedGesture.hpp"
#include "Shared.hpp"

struct DragGestureEvent {
    uint32_t time;
    GestureType type;
    GestureDirection direction;
    uint32_t finger_count;

    GestureDirection edge_origin;

    std::string to_string() const;
};
