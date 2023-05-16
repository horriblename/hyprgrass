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
    Test tests[] = {
        {{MONITOR_X + 10, MONITOR_Y + 10},
         GESTURE_DIRECTION_DOWN | GESTURE_DIRECTION_RIGHT},
        {{MONITOR_X, MONITOR_Y + 11}, GESTURE_DIRECTION_RIGHT},
        {{MONITOR_X + 11, MONITOR_Y}, GESTURE_DIRECTION_DOWN},
        {{MONITOR_X + 11, MONITOR_Y + 11}, 0},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT},
         GESTURE_DIRECTION_UP | GESTURE_DIRECTION_LEFT},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT},
         GESTURE_DIRECTION_UP},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT - 11},
         GESTURE_DIRECTION_LEFT},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT - 11}, 0},
    };

    CMockGestureManager mockGM;
    for (auto& test : tests) {
        assert(mockGM.find_swipe_edges(test.origin) == test.result);
    }
    return true;
}
