/**
 * If you can't already tell, I don't know much about C++ so I would like you to
 * divert your attention away from this atrocity. Thank you.
 *
 */
#include "MockGestureManager.hpp"
#include "wayfire/touch/touch.hpp"
#include <bits/stdc++.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

inline bool only_one(bool a, bool b, bool c) {
    return (a && !b && !c) || (!a && b && !c) || (!a && !b && c);
}

bool testFile(CMockGestureManager* mockGM, std::string fname);

bool testWorkspaceSwipeBegin() {
    CMockGestureManager mockGM;
    mockGM.addWorkspaceSwipeBeginGesture();

    return testFile(&mockGM, "test/cases/swipeLeft.csv");
}

bool testWorkspaceSwipeTimeout() {
    CMockGestureManager mockGM;
    mockGM.addWorkspaceSwipeBeginGesture();

    return testFile(&mockGM, "test/cases/swipeTimeout.csv");
}

bool testEdgeSwipe() {
    CMockGestureManager mockGM;
    mockGM.addEdgeSwipeGesture();
    return testFile(&mockGM, "test/cases/edgeLeft.csv");
}

bool testEdgeSwipeTimeout() {
    CMockGestureManager mockGM;
    mockGM.addEdgeSwipeGesture();
    return testFile(&mockGM, "test/cases/edgeSwipeTimeout.csv");
}

bool testEdgeReleaseTimeout() {
    CMockGestureManager mockGM;
    mockGM.addEdgeSwipeGesture();
    return testFile(&mockGM, "test/cases/edgeReleaseTimeout.csv");
}

bool testEdgeInvalidStart() {
    CMockGestureManager mockGM;
    mockGM.addEdgeSwipeGesture();
    return testFile(&mockGM, "test/cases/edgeInvalidStart.csv");
}

// @return true if passed
bool testFile(CMockGestureManager* mockGM, std::string fname) {
    auto infile = std::ifstream(fname);
    assert(infile.good());
    std::string type, line;
    uint32_t time;
    int id;
    double x, y;
    bool running = true;
    std::cout << std::setprecision(2);

    while (std::getline(infile, line)) {
        std::istringstream linestream(line);
        linestream >> type;
        if (type == "DOWN") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id >> x >> y;

            const wf::touch::gesture_event_t ev = {
                wf::touch::EVENT_TYPE_TOUCH_DOWN, time, id, {x, y}};
            mockGM->onTouchDown(ev);
        } else if (type == "UP") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id;

            auto lift_off_pos = mockGM->getLastPositionOfFinger(id);
            const wf::touch::gesture_event_t ev = {
                wf::touch::EVENT_TYPE_TOUCH_UP, time, id, lift_off_pos};
            mockGM->onTouchUp(ev);
        } else if (type == "MOVE") {
            assert(only_one(running, mockGM->triggered, mockGM->cancelled));
            linestream >> time >> id >> x >> y;

            const wf::touch::gesture_event_t ev = {
                wf::touch::EVENT_TYPE_MOTION, time, id, {x, y}};
            mockGM->onTouchMove(ev);
        } else if (type == "CHECK") {
            linestream >> type;
            if (type == "complete") {
                running = false;
                assert(mockGM->triggered);
            } else if (type == "cancel") {
                running = false;
                assert(mockGM->cancelled);
            } else if (type == "progress") {
                double actual_progress =
                    mockGM->getGestureAt(0)->get()->get_progress();
                double progress;
                linestream >> progress;
                std::cout << "CHECK progress: " << actual_progress
                          << ", status: ";
                std::cout << (running ? "running" : "")
                          << (mockGM->triggered ? "triggered" : "")
                          << (mockGM->cancelled ? "cancelled" : "") << "\n";

                assert(actual_progress == progress);
            } else {
                std::cout << "unknown argument to CHECK: " << type;
                assert(false);
            }
        } else if (type == "#") {
            // comment
        } else {
            std::cout << "invalid COMMAND in test file " << fname << ": "
                      << type << "\n";
            assert(false);
        }
    }

    return true;
}

int main() {
    Tester tester;
    if (tester.testFindSwipeEdges())
        std::cout << "passed test for find_swipe_edges()\n";
    else
        return 1;

    if (testWorkspaceSwipeBegin())
        std::cout << "passed test #1\n";
    else
        return 1;

    if (testWorkspaceSwipeTimeout())
        std::cout << "passed test #2\n";
    else
        return 2;

    if (testEdgeSwipe())
        std::cout << "passed test #3\n";
    else
        return 3;

    if (testEdgeSwipeTimeout())
        std::cout << "passed test #4\n";
    else
        return 4;

    if (testEdgeReleaseTimeout())
        std::cout << "passed test #5\n";
    else
        return 5;

    if (testEdgeInvalidStart())
        std::cout << "passed test #6\n";
    else
        return 6;

    return 0;
}
