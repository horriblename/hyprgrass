#include "ShimTrackpadGestures.hpp"
#include "debug/log/Logger.hpp"
#include "gestures/DragGesture.hpp"
#include "gestures/Shared.hpp"
#include "managers/input/trackpad/GestureTypes.hpp"
#include "managers/input/trackpad/TrackpadGestures.hpp"
#include <string>

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

bool ShimTrackpadGestures::isSingleDirection(eTrackpadGestureDirection dir) {
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

bool ShimTrackpadGestures::isPinch(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_PINCH:
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
        case TRACKPAD_GESTURE_DIR_PINCH_OUT:
            return true;
        default:
            return false;
    }
}

std::expected<GestureConfig, std::string> parseGesturePattern(Hyprutils::String::CConstVarList& vars) {
    GestureType type;
    size_t fingersOrOrigin              = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    if (vars.size() < 4) {
        return std::unexpected("hyprgrass-gesture must have at least 4 arguments");
    }

    if (vars[0] == "swipe") {
        type = GestureType::SWIPE;

        auto res = parseFingers(vars[1], fingersOrOrigin);
        if (!res) {
            return std::unexpected(res.error());
        }

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for a swipe gesture: {}", vars[2]));
        }
    } else if (vars[0] == "edge") {
        type        = GestureType::EDGE_SWIPE;
        auto origin = g_pTrackpadGestures->dirForString(vars[1]);
        if (!ShimTrackpadGestures::isSingleDirection(origin)) {
            return std::unexpected(
                std::format("invalid ORIGIN for an edge gesture, expected a single direction, got {}", vars[1])
            );
        }

        fingersOrOrigin = toHyprgrassDirection(origin);

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for an edge gesture: {}", vars[2]));
        }
    } else if (vars[0] == "longpress") {
        type     = GestureType::LONG_PRESS;
        auto res = parseFingers(vars[1], fingersOrOrigin);
        if (!res) {
            return std::unexpected(res.error());
        }

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        // // pinch disabled for now for being buggy
        // } else if (vars[0] == "pinch") {
        //     type = GestureType::PINCH;
        //     auto res = parseFingers(vars[1], fingersOrOrigin);
        //     if (!res) {
        //         return std::unexpected(res.error());
        //     }
        //
        //     direction = g_pTrackpadGestures->dirForString(vars[2]);
        //     if (!isPinch(direction)) {
        //         return std::unexpected(std::format("empty or invalid direction for a pinch gesture: {}", vars[2]));
        //     }
    } else {
        return std::unexpected(std::format("invalid gesture event: {}", vars[0]));
    }

    return GestureConfig{
        .type            = type,
        .direction       = direction,
        .fingersOrOrigin = static_cast<size_t>(fingersOrOrigin),
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
            return GESTURE_DIRECTION_IN | GESTURE_DIRECTION_OUT;
        case TRACKPAD_GESTURE_DIR_PINCH_OUT:
            return GESTURE_DIRECTION_OUT;
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
            return GESTURE_DIRECTION_IN;
    }

    return 0;
}

static void printGesture(GestureType type, const CTrackpadGestures::SGestureData& gesture) {
    switch (type) {
        case GestureType::SWIPE: {
            std::string direction = stringifyDirection(toHyprgrassDirection(gesture.direction));
            Log::logger->log(
                Log::DEBUG, "| gesture: swipe, fingers: {}, direction: {}", gesture.fingerCount, direction
            );
            break;
        }
        case GestureType::LONG_PRESS:
            Log::logger->log(Log::DEBUG, "| gesture: long_press, fingers: {}", gesture.fingerCount);
            break;
        case GestureType::EDGE_SWIPE: {
            std::string origin    = stringifyDirection(gesture.fingerCount);
            std::string direction = stringifyDirection(toHyprgrassDirection(gesture.direction));
            Log::logger->log(Log::DEBUG, "| gesture: edge, origin: {}, direction: {}", origin, direction);
            break;
        }
        case GestureType::PINCH:
            Log::logger->log(Log::DEBUG, "| gesture: long_press, fingers: {}", gesture.fingerCount);
            break;
    }

    // TODO: pretty print this
    Log::logger->log(Log::DEBUG, "| mod mask: {}", gesture.modMask);
    Log::logger->log(Log::DEBUG, "| scale: {}", gesture.deltaScale);
    Log::logger->log(Log::DEBUG, "| disable inhibit: {}", gesture.disableInhibit);
    Log::logger->log(Log::DEBUG, "|");
}

void ShimTrackpadGestures::listGestures() {
    Log::logger->log(Log::DEBUG, "[hyprgrass] listing gestures:");

    const auto types = std::array{
        GestureType::SWIPE,
        GestureType::LONG_PRESS,
        GestureType::EDGE_SWIPE,
        GestureType::PINCH,
    };

    for (const GestureType type : types) {
        for (const auto& gesture : this->get(type)->m_gestures) {
            printGesture(type, *gesture);
        }
    }
}
