#include "DragGesture.hpp"

std::string DragGesture::to_string() const {
    switch (type) {
        case DragGestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
        case DragGestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
        case DragGestureType::EDGE_SWIPE:
            return "edge:" + stringifyDirection(this->edge_origin) + ":" + stringifyDirection(this->direction);
    }

    return "";
}
