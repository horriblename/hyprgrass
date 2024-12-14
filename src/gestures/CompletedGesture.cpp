#include "CompletedGesture.hpp"
#include "Shared.hpp"

std::string CompletedGestureEvent::to_string() const {
    switch (type) {
        case CompletedGestureType::EDGE_SWIPE:
            return "edge:" + stringifyDirection(this->edge_origin) + ":" + stringifyDirection(this->direction);
        case CompletedGestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
            break;
        case CompletedGestureType::TAP:
            return "tap:" + std::to_string(finger_count);
        case CompletedGestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
        case CompletedGestureType::PINCH:
            return "pinch:" + std::to_string(finger_count) + ":" + stringifyPinchDirection(this->pinch_direction);
    }

    return "";
}
