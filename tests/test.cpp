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

// @return true if passed
bool testFile(std::string fname) {
    std::ifstream infile(fname);
    std::string type, line;
    uint32_t time;
    int id;
    double x, y;
    CMockGestureManager mockGM;

    while (std::getline(infile, line)) {
        std::istringstream linestream(line);
        linestream >> type;
        if (type == "DOWN") {
            linestream >> time >> id >> x >> y;

            wlr_touch_down_event ev = {
                .time_msec = time, .touch_id = id, .x = x, .y = y};
            mockGM.onTouchDown(&ev);
        } else if (type == "UP") {
            linestream >> time >> id;

            wlr_touch_up_event ev = {.time_msec = time, .touch_id = id};
            mockGM.onTouchUp(&ev);
        } else if (type == "MOVE") {
            linestream >> time >> id >> x >> y;

            wlr_touch_motion_event ev = {
                .time_msec = time, .touch_id = id, .x = x, .y = y};
            mockGM.onTouchMove(&ev);
        } else if (type == "CHECK") {
            linestream >> type;
            if (type == "complete") {
                assert(mockGM.workspaceSwipeTriggered);
            } else {
                assert(mockGM.workspaceSwipeCancelled);
            }
        } else {
            assert(false);
        }
    }

    return true;
}

int main() {
    if (testFile("tests/cases/swipeLeft.csv"))
        std::cout << "passed test #1";
    else
        return 1;

    if (testFile("tests/cases/swipeTimeout.csv"))
        std::cout << "passed test #2";
    else
        return 2;

    return 0;
}
