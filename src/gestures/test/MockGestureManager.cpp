#include "MockGestureManager.hpp"
#include "../Gestures.hpp"
#include <cassert>
#include <iostream>

#define CONFIG_SENSITIVITY 1.0

CMockGestureManager::CMockGestureManager() {}

void CMockGestureManager::handleGesture(const CompletedGesture& gev) {
    std::cout << "gesture triggered: " << gev.to_string() << "\n";
    this->triggered = true;
}

void CMockGestureManager::handleCancelledGesture() {
    std::cout << "gesture cancelled\n";
    this->cancelled = true;
}

bool Tester::testFindSwipeEdges() {
    using Test = struct {
        wf::touch::point_t origin;
        gestureDirection result;
    };
    const auto L = GESTURE_DIRECTION_LEFT;
    const auto R = GESTURE_DIRECTION_RIGHT;
    const auto U = GESTURE_DIRECTION_UP;
    const auto D = GESTURE_DIRECTION_DOWN;

    Test tests[] = {
        {{MONITOR_X + 10, MONITOR_Y + 10}, D | R},
        {{MONITOR_X, MONITOR_Y + 11}, R | U | D},
        {{MONITOR_X + 11, MONITOR_Y}, D | L | R},
        {{MONITOR_X + 11, MONITOR_Y + 11}, 0},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT}, U | L},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT},
         U | L | R},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT - 11},
         L | U | D},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT - 11}, 0},
    };

    CMockGestureManager mockGM;
    for (auto& test : tests) {
        assert(mockGM.find_swipe_edges(test.origin) == test.result);
    }
    return true;
}
