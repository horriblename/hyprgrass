#include "ShimTrackpadGestures.hpp"
#include "gestures/Shared.hpp"
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <lua.h>
#include <string>
#include <string_view>
#include <vector>

static std::expected<size_t, std::string> parseFingers(const std::string_view& s) {
    size_t fingers;
    auto result = std::from_chars(s.data(), s.data() + s.size(), fingers);
    if (result.ec == std::errc::invalid_argument) {
        return std::unexpected(
            std::format("invalid gesture pattern: expected an integer finger count, got {}", s)
        );
    } else if (result.ec == std::errc::result_out_of_range) {
        return std::unexpected("finger count too large/too small");
    }
    return {fingers};
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

bool ShimTrackpadGestures::isSinglePinchDirection(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_PINCH_OUT:
        case TRACKPAD_GESTURE_DIR_PINCH_IN:
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

std::expected<GesturePattern, std::string> parseGesturePattern(const std::string_view& s) {
    GestureType type;
    size_t fingersOrOrigin              = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    std::vector<std::string> vars;
    for (size_t start = 0;;) {
        const size_t end = s.find(':', start);
        if (end == std::string_view::npos) {
            vars.emplace_back(s.substr(start));
            break;
        }
        vars.emplace_back(s.substr(start, end - start));
        start = end + 1;
    }

    if (vars.size() < 2) {
        return std::unexpected("invalid pattern string: expected at least 2 segments");
    }

    if (vars[0] == "swipe") {
        type = GestureType::SWIPE;
        if (vars.size() != 3) {
            return std::unexpected(std::format("invalid pattern string: '{}', expected 3 segments", s));
        }

        auto res = parseFingers(vars[1]);
        if (!res) {
            return std::unexpected(res.error());
        }
        fingersOrOrigin = res.value();

        direction = g_pTrackpadGestures->dirForString(vars[2]);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for a swipe gesture: {}", vars[2]));
        }
    } else if (vars[0] == "edge") {
        type           = GestureType::EDGE_SWIPE;
        int i          = 1;
        size_t fingers = 1;
        if (vars.size() < 3 || vars.size() > 4) {
            return std::unexpected(std::format("invalid pattern string: '{}', expected 3 or 4 segments", s));
        }

        auto res = parseFingers(vars[i]);
        if (res) {
            fingers = res.value();
            if (fingers > FINGERS_MASK) {
                return std::unexpected(std::format("finger count too large for an edge gesture: {}", fingers));
            }
            i++;
            if (vars.size() != 4) {
                return std::unexpected(
                    std::format(
                        "invalid pattern string: '{}', edge pattern with finger count must have 4 segments", s
                    )
                );
            }
        } else if (vars.size() != 3) {
            return std::unexpected(
                std::format(
                    "invalid pattern string: '{}', edge pattern with no finger count must have 3 segments", s
                )
            );
        }

        auto origin = g_pTrackpadGestures->dirForString(vars[i]);
        if (!ShimTrackpadGestures::isSingleDirection(origin)) {
            return std::unexpected(
                std::format("invalid ORIGIN for an edge gesture, expected a single direction, got {}", vars[i])
            );
        }
        i++;

        fingersOrOrigin = (static_cast<size_t>(toHyprgrassDirection(origin)) << MOD_MASK_SHIFT) | fingers;

        direction = g_pTrackpadGestures->dirForString(vars[i]);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE) {
            return std::unexpected(std::format("invalid direction for an edge gesture: {}", vars[i]));
        }
    } else if (vars[0] == "longpress") {
        type = GestureType::LONG_PRESS;
        if (vars.size() < 2 || vars.size() > 3) {
            return std::unexpected(std::format("invalid pattern string: '{}', expected 2 or 3 segments", s));
        }

        auto res = parseFingers(vars[1]);
        if (!res) {
            return std::unexpected(res.error());
        }
        fingersOrOrigin = res.value();

        if (vars.size() == 3)
            direction = g_pTrackpadGestures->dirForString(vars[2]);
    } else if (vars[0] == "pinch") {
        type = GestureType::PINCH;
        if (vars.size() != 3) {
            return std::unexpected(std::format("invalid pattern string: '{}', expected 3 segments", s));
        }

        auto res = parseFingers(vars[1]);
        if (!res) {
            return std::unexpected(res.error());
        }
        fingersOrOrigin = res.value();

        if (vars[2] == "i" || vars[2] == "in")
            direction = TRACKPAD_GESTURE_DIR_PINCH_IN;
        else if (vars[2] == "o" || vars[2] == "out")
            direction = TRACKPAD_GESTURE_DIR_PINCH_OUT;
        else
            direction = g_pTrackpadGestures->dirForString(vars[2]);

        if (!ShimTrackpadGestures::isSinglePinchDirection(direction)) {
            return std::unexpected(std::format("invalid direction for a pinch gesture: {}", vars[2]));
        }
    } else if (vars[0] == "tap") {
        type = GestureType::TAP;
        if (vars.size() != 2) {
            return std::unexpected(std::format("invalid pattern string: '{}', expected 2 segments", s));
        }

        auto res = parseFingers(vars[1]);
        if (!res) {
            return std::unexpected(res.error());
        }
        fingersOrOrigin = res.value();
    } else {
        return std::unexpected(std::format("invalid gesture event: {}", vars[0]));
    }

    return GesturePattern{
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
            Log::logger->log(Log::DEBUG, "| kind: swipe, fingers: {}, direction: {}", gesture.fingerCount, direction);
            break;
        }
        case GestureType::LONG_PRESS:
            Log::logger->log(Log::DEBUG, "| kind: long_press, fingers: {}", gesture.fingerCount);
            break;
        case GestureType::EDGE_SWIPE: {
            std::string origin    = stringifyDirection(gesture.fingerCount >> MOD_MASK_SHIFT);
            uint32_t fingers      = gesture.fingerCount & FINGERS_MASK;
            std::string direction = stringifyDirection(toHyprgrassDirection(gesture.direction));
            Log::logger->log(
                Log::DEBUG, "| kind: edge, origin: {}, fingers: {}, direction: {}", origin, fingers, direction
            );
            break;
        }
        case GestureType::PINCH:
            Log::logger->log(Log::DEBUG, "| kind: long_press, fingers: {}", gesture.fingerCount);
            break;
        case GestureType::TAP:
            Log::logger->log(Log::DEBUG, "| kind: tap, fingers: {}", gesture.fingerCount);
            break;
    }

    // TODO: pretty print this
    Log::logger->log(Log::DEBUG, "| mod mask: {}", static_cast<uint32_t>(gesture.modMask));
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
