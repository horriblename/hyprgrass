#include "ShimTrackpadGestures.hpp"
#include <charconv>

static std::expected<void, std::string> parseFingers(const std::string_view& s, size_t& fingers) {
    auto result = std::from_chars(s.data(), s.data() + s.size(), fingers);
    if (result.ec == std::errc::invalid_argument) {
        return std::unexpected(
            std::format("invalid swipe gesture pattern: second segment must be an integer, got {}", s)
        );
    } else if (result.ec == std::errc::result_out_of_range) {
        return std::unexpected("finger count too large/too small");
    }
    return {};
}

static bool isSingleDirection(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_LEFT:
        case TRACKPAD_GESTURE_DIR_RIGHT:
        case TRACKPAD_GESTURE_DIR_UP:
        case TRACKPAD_GESTURE_DIR_DOWN:
            return true;
        default:
            return false;
    }
}

static bool isPinch(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_PINCH:
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
        case TRACKPAD_GESTURE_DIR_PINCH_OUT:
            return true;
        default:
            return false;
    }
}

std::expected<GestureConfig, std::string> parseGesturePattern(CConstVarList& vars) {
    DragGestureType type;
    size_t fingersOrOrigin              = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    if (vars.size() < 4) {
        return std::unexpected("hyprgrass-gesture must have at least 4 arguments");
    }

    if (vars[0] == "swipe") {
        type = DragGestureType::SWIPE;

        auto res = parseFingers(vars[1], fingersOrOrigin);
        if (!res) {
            return std::unexpected(res.error());
        }

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for a swipe gesture: {}", vars[2]));
        }
    } else if (vars[0] == "edge") {
        type        = DragGestureType::EDGE_SWIPE;
        auto origin = g_pTrackpadGestures->dirForString(vars[1]);
        if (!isSingleDirection(origin)) {
            return std::unexpected(
                std::format("invalid ORIGIN for an edge gesture, expected a single direction, got {}", vars[1])
            );
        }

        fingersOrOrigin = toHyprgrassDirection(origin);

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for an edge gesture: {}", vars[2]));
        }
    } else if (vars[0] == "longpress") {
        type     = DragGestureType::LONG_PRESS;
        auto res = parseFingers(vars[1], fingersOrOrigin);
        if (!res) {
            return std::unexpected(res.error());
        }

        direction = g_pTrackpadGestures->dirForString(vars[2]);
    } else {
        return std::unexpected(std::format("invalid gesture event: {}", vars[0]));
    }

    return GestureConfig{
        .type      = type,
        .direction = direction,
        .fingers   = static_cast<size_t>(fingersOrOrigin),
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
