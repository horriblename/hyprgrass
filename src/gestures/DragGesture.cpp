#include "DragGesture.hpp"
#include "Shared.hpp"
#include <string>

std::string DragGestureEvent::to_string() const {
    switch (type) {
        case GestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
        case GestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
        case GestureType::EDGE_SWIPE:
            return "edge:" + stringifyDirection(this->edge_origin) + ":" + stringifyDirection(this->direction);
        case GestureType::PINCH:
            return "pinch:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
    }

    return "";
}
