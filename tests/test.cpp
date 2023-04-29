/**
 * If you can't already tell, I don't know much about C++ so I would like you to
 * divert your attention away from this atrocity. Thank you.
 *
 */
#include "MockGestureManager.hpp"
#include "wayfire/touch/touch.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>

inline bool only_one(bool a, bool b, bool c) {
    return (a && !b && !c) || (!a && b && !c) || (!a && !b && c);
}

bool testFile(CMockGestureManager* mockGM, std::string fname);

bool testWorkspaceSwipeBegin() {
    CMockGestureManager mockGM;
    mockGM.addWorkspaceSwipeBeginGesture();

    return testFile(&mockGM, "tests/cases/swipeLeft.csv");
}

bool testWorkspaceSwipeTimeout() {
    CMockGestureManager mockGM;
    mockGM.addWorkspaceSwipeBeginGesture();

    return testFile(&mockGM, "tests/cases/swipeTimeout.csv");
}

// @return true if passed
bool testFile(CMockGestureManager* mockGM, std::string fname) {
    std::ifstream infile(fname);
    std::string type, line;
    uint32_t time;
    int id;
    double x, y;
    bool running = true;

    while (std::getline(infile, line)) {
        std::istringstream linestream(line);
        linestream >> type;
        if (type == "DOWN") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id >> x >> y;

            wlr_touch_down_event ev = {
                .time_msec = time, .touch_id = id, .x = x, .y = y};
            mockGM->onTouchDown(&ev);
        } else if (type == "UP") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id;

            wlr_touch_up_event ev = {.time_msec = time, .touch_id = id};
            mockGM->onTouchUp(&ev);
        } else if (type == "MOVE") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id >> x >> y;

            wlr_touch_motion_event ev = {
                .time_msec = time, .touch_id = id, .x = x, .y = y};
            mockGM->onTouchMove(&ev);
        } else if (type == "CHECK") {
            linestream >> type;
            if (type == "complete") {
                running = false;
                assert(mockGM->triggered);
            } else {
                running = false;
                assert(mockGM->cancelled);
            }
        } else {
            assert(false);
        }
    }

    return true;
}

int main() {
    if (testWorkspaceSwipeBegin())
        std::cout << "passed test #1\n";
    else
        return 1;

    if (testWorkspaceSwipeTimeout())
        std::cout << "passed test #2\n";
    else
        return 2;

    return 0;
}
