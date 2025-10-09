#include "ShimTrackpadGestures.hpp"
#include <cstring>

std::expected<GestureConfig, std::string> parseGesturePattern(std::string p) {
    auto vars = CVarList(p, 0, ':');
    DragGestureType type;
    int fingers                         = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    if (vars.size() < 1) {
        return std::unexpected(std::format("invalid gesture pattern: {}", p));
    }

    if (vars[0] == "swipe") {
        type = DragGestureType::SWIPE;
        if (vars.size() != 3) {
            return std::unexpected("invalid swipe gesture pattern: " + p);
        }

        try {
            fingers = std::stoi(vars[1]);
        } catch (std::invalid_argument const&) {
            return std::unexpected(
                std::format("invalid swipe gesture pattern: {}, second segment must be an integer", p)
            );
        }

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (direction == TRACKPAD_GESTURE_DIR_NONE)
            direction = TRACKPAD_GESTURE_DIR_SWIPE; // TODO: do I need this
    } else if (vars[0] == "edge") {
        if (vars.size() != 3) {
            return std::unexpected("invalid edge gesture pattern: " + p);
        }

        type = DragGestureType::EDGE_SWIPE;
        switch (vars[1][0]) {
            case 'u':
                fingers = GESTURE_DIRECTION_UP;
                break;
            case 'd':
                fingers = GESTURE_DIRECTION_DOWN;
                break;
            case 'l':
                fingers = GESTURE_DIRECTION_LEFT;
                break;
            case 'r':
                fingers = GESTURE_DIRECTION_RIGHT;
                break;
            default:
                return std::unexpected(
                    std::format("invalid edge gesture pattern: {}, second segment must be an u, d, l or r", p)
                );
        }
        direction = g_pTrackpadGestures->dirForString(vars[2]);
    } else if (vars[0] == "longpress") {
        type = DragGestureType::LONG_PRESS;
        try {
            fingers   = std::stoi(vars[1]);
            direction = TRACKPAD_GESTURE_DIR_SWIPE;
        } catch (std::invalid_argument const&) {
            return std::unexpected(std::format("invalid longpress gesture pattern: {}, argument must be an integer", p)
            );
        }
    } else {
        return std::unexpected("invalid gesture pattern: " + p);
    }

    return GestureConfig{
        .type      = type,
        .direction = direction,
        .fingers   = static_cast<size_t>(fingers),
    };
}

GestureDirection toHyprgrassDirection(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_NONE:
            return 0;
        case TRACKPAD_GESTURE_DIR_SWIPE:
            return GESTURE_DIRECTION_LEFT | GESTURE_DIRECTION_RIGHT | GESTURE_DIRECTION_UP | GESTURE_DIRECTION_DOWN;
        case TRACKPAD_GESTURE_DIR_LEFT:
            return GESTURE_DIRECTION_LEFT;
        case TRACKPAD_GESTURE_DIR_RIGHT:
            return GESTURE_DIRECTION_RIGHT;
        case TRACKPAD_GESTURE_DIR_UP:
            return GESTURE_DIRECTION_UP;
        case TRACKPAD_GESTURE_DIR_DOWN:
            return GESTURE_DIRECTION_DOWN;
        case TRACKPAD_GESTURE_DIR_VERTICAL:
            return GESTURE_DIRECTION_UP | GESTURE_DIRECTION_DOWN;
        case TRACKPAD_GESTURE_DIR_HORIZONTAL:
            return GESTURE_DIRECTION_LEFT | GESTURE_DIRECTION_RIGHT;
        case TRACKPAD_GESTURE_DIR_PINCH:
            return 0; // TODO
        case TRACKPAD_GESTURE_DIR_PINCH_OUT:
            return 0; // TODO
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
            return 0; // TODO
    }

    return 0;
}
