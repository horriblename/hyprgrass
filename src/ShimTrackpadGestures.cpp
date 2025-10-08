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
        if (vars.size() != 2) {
            return std::unexpected("invalid swipe gesture pattern: " + p);
        }

        try {
            fingers = std::stoi(vars[1]);
        } catch (std::invalid_argument const&) {
            return std::unexpected(std::format("invalid swipe gesture pattern: {}, argument must be an integer", p));
        }
    } else if (vars[0] == "edge") {
        type    = DragGestureType::EDGE_SWIPE;
        fingers = 1;
    } else if (vars[0] == "longpress") {
        type = DragGestureType::LONG_PRESS;
        try {
            fingers = std::stoi(vars[1]);
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
