#pragma once
#include <cstdint>
#include <string>

// Swipe params
constexpr static double MIN_SWIPE_DISTANCE             = 30;
constexpr static double MAX_SWIPE_DISTANCE             = 450;
constexpr static double SWIPE_INCORRECT_DRAG_TOLERANCE = 150;

// Pinch params
// based on libinput's parameters (PINCH_DISAMBIGUATION_MOVE_THRESHOLD is three times the amount
// of HOLD_AND_MOTION_THRESHOLD but they are technically "squared" values so 150*sqrt(3) ~ 260)
constexpr static double PINCH_INCORRECT_DRAG_TOLERANCE = 260;

// Hold params
constexpr static double HOLD_INCORRECT_DRAG_TOLERANCE = 100;

// General
constexpr static double GESTURE_INITIAL_TOLERANCE = 40;
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
};

enum class PinchDirection {
    IN,
    OUT,
};

std::string stringifyDirection(GestureDirection direction);
std::string stringifyPinchDirection(PinchDirection direction);
