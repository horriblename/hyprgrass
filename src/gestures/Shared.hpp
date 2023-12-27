#pragma once
#include <cstdint>

// Swipe params
constexpr static int EDGE_SWIPE_THRESHOLD              = 10;
constexpr static double MIN_SWIPE_DISTANCE             = 30;
constexpr static double MAX_SWIPE_DISTANCE             = 450;
constexpr static double SWIPE_INCORRECT_DRAG_TOLERANCE = 150;

// Pinch params
constexpr static double PINCH_INCORRECT_DRAG_TOLERANCE = 200;
constexpr static double PINCH_THRESHOLD                = 1.5;

// Hold params
constexpr static double HOLD_INCORRECT_DRAG_TOLERANCE = 100;

// General
constexpr static double GESTURE_INITIAL_TOLERANCE = 40;
constexpr static uint32_t GESTURE_BASE_DURATION   = 400;

constexpr static uint32_t SEND_CANCEL_EVENT_FINGER_COUNT = 3;

// can be one of @eTouchGestureDirection or a combination of them
using GestureDirection = uint32_t;
