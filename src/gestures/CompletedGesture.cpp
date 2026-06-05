#include "CompletedGesture.hpp"
#include "Shared.hpp"

std::string CompletedGestureEvent::to_string() const {
    switch (type) {
        case GestureType::EDGE_SWIPE:
            return "edge:" + stringifyDirection(this->edge_origin) + ":" + stringifyDirection(this->direction);
        case GestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
            break;
        case GestureType::TAP:
            return "tap:" + std::to_string(finger_count);
        case GestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
        case GestureType::PINCH:
            return "pinch:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
    }

    return "";
}
