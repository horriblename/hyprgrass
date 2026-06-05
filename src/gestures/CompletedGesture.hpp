#pragma once
#include "Shared.hpp"
#include <string>

enum class GestureType {
    // Invalid Gesture
    SWIPE,
    EDGE_SWIPE,
    LONG_PRESS,
    PINCH,
    TAP,
};

std::string stringifyGestureType(const GestureType&);

struct CompletedGestureEvent {
    GestureType type;
    GestureDirection direction;
    uint32_t finger_count;

    // TODO turn this whole struct into a sum type?
    // edge swipe specific
    GestureDirection edge_origin;

    std::string to_string() const;
    inline bool operator==(const CompletedGestureEvent& other) {
        return type == other.type && direction == other.direction && finger_count == other.finger_count &&
               edge_origin == other.edge_origin;
    }
};
