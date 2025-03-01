#pragma once
#include <cstdint>
#include <string>

// Swipe params
constexpr static double MIN_SWIPE_DISTANCE             = 4;
constexpr static double MAX_SWIPE_DISTANCE             = 44;
constexpr static double SWIPE_INCORRECT_DRAG_TOLERANCE = 4;

// Pinch params
constexpr static double PINCH_INCORRECT_DRAG_TOLERANCE = 28;
constexpr static double PINCH_THRESHOLD                = 1.5;

// Hold params
constexpr static double HOLD_INCORRECT_DRAG_TOLERANCE = 14;

// General
constexpr static double GESTURE_INITIAL_TOLERANCE = 6;
constexpr static uint32_t GESTURE_BASE_DURATION   = 400;

constexpr static uint32_t SEND_CANCEL_EVENT_FINGER_COUNT = 3;

// can be one of @eTouchGestureDirection or a combination of them
using GestureDirection = uint32_t;

enum TouchGestureDirection {
    /* Swipe-specific */
    GESTURE_DIRECTION_LEFT  = (1 << 0),
    GESTURE_DIRECTION_RIGHT = (1 << 1),
    GESTURE_DIRECTION_UP    = (1 << 2),
    GESTURE_DIRECTION_DOWN  = (1 << 3),
    /* Pinch-specific */
    // GESTURE_DIRECTION_IN = (1 << 4),
    // GESTURE_DIRECTION_OUT = (1 << 5),
};

std::string stringifyDirection(GestureDirection direction);
