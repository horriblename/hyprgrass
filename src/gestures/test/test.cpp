#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "MockGestureManager.hpp"
#include "wayfire/touch/touch.hpp"
#include <variant>
#include <vector>

constexpr float SENSITIVITY = 1.0;

void Tester::testFindSwipeEdges() {
    using Test = struct {
        wf::touch::point_t origin;
        gestureDirection result;
    };
    const auto L = GESTURE_DIRECTION_LEFT;
    const auto R = GESTURE_DIRECTION_RIGHT;
    const auto U = GESTURE_DIRECTION_UP;
    const auto D = GESTURE_DIRECTION_DOWN;

    Test tests[] = {
        {{MONITOR_X + 10, MONITOR_Y + 10}, U | L},
        {{MONITOR_X, MONITOR_Y + 11}, L},
        {{MONITOR_X + 11, MONITOR_Y}, U},
        {{MONITOR_X + 11, MONITOR_Y + 11}, 0},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT}, D | R},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT}, D},
        {{MONITOR_X + MONITOR_WIDTH, MONITOR_Y + MONITOR_HEIGHT - 11}, R},
        {{MONITOR_X + MONITOR_WIDTH - 11, MONITOR_Y + MONITOR_HEIGHT - 11}, 0},
    };

    CMockGestureManager mockGM;
    for (auto& test : tests) {
        CHECK_EQ(mockGM.find_swipe_edges(test.origin), test.result);
    }
}

enum class ExpectResultType {
    COMPLETED,
    CANCELLED,
    CHECK_PROGRESS,
};

struct ExpectResult {
    ExpectResultType type;

    // variables below only used when type == CHECK_PROGRESS
    float progress = 0.0;

    // which of the m_vGestures to use
    // usually the first (0)
    int gesture_index = 0;
};

// NOTE each event implicitly means the gesture has not been triggered/cancelled
// yet
void ProcessEvent(CMockGestureManager& gm,
                  const wf::touch::gesture_event_t& ev) {
    CHECK_FALSE(gm.triggered);
    CHECK_FALSE(gm.cancelled);
    switch (ev.type) {
        case wf::touch::EVENT_TYPE_TOUCH_DOWN:
            gm.onTouchDown(ev);
            break;
        case wf::touch::EVENT_TYPE_TOUCH_UP:
            gm.onTouchUp(ev);
            break;
        case wf::touch::EVENT_TYPE_MOTION:
            gm.onTouchMove(ev);
            break;
    }
}

void ProcessEvents(CMockGestureManager& gm, ExpectResult expect,
                   const std::vector<wf::touch::gesture_event_t>& events) {
    for (const auto& ev : events) {
        ProcessEvent(gm, ev);
    }

    switch (expect.type) {
        case ExpectResultType::COMPLETED:
            CHECK(gm.triggered);
            break;
        case ExpectResultType::CANCELLED:
            CHECK(gm.cancelled);
            break;
        case ExpectResultType::CHECK_PROGRESS:
            CHECK(
                gm.getGestureAt(expect.gesture_index)->get()->get_progress() ==
                expect.progress);
    }
}

using TouchEvent = wf::touch::gesture_event_t;
using wf::touch::point_t;

TEST_CASE("Swipe Drag: Complete upon moving more than the threshold") {
    CMockGestureManager gm;
    gm.addMultiFingerDragGesture(&SENSITIVITY);
    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {550, 290}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 0, {0, 290}},
        /* FIXME idk why this triggers after one finger movement, should be
        after all finers moved? threshold should be 450, I thought the center is
        used for distance? */
        // {wf::touch::EVENT_TYPE_MOTION, 200, 1, point_t{50, 300}},
        // {wf::touch::EVENT_TYPE_MOTION, 200, 2, point_t{100, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

TEST_CASE("Swipe Drag: Cancel 2 finger swipe due to moving too much before "
          "adding new finger, but not enough to trigger 2 finger swipe first") {
    CMockGestureManager gm;
    gm.addMultiFingerDragGesture(&SENSITIVITY);
    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 110, 0, {409, 290}},
        {wf::touch::EVENT_TYPE_MOTION, 110, 1, {459, 300}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 150, 2, {401, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Swipe: Complete upon moving more than the threshold then lifting a "
          "finger") {
    CMockGestureManager gm;
    gm.addMultiFingerSwipeThenLiftoffGesture(&SENSITIVITY);

    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {550, 290}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 0, {0, 290}},
        // FIXME same issue as in Swipe Drag
        // TODO CHECK progress == 0.5
        {wf::touch::EVENT_TYPE_MOTION, 200, 1, {50, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 2, {100, 290}},
        {wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {100, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

TEST_CASE("Edge Swipe: Complete upon: \n"
          "1. touch down on edge of screen\n"
          "2. swiping more than the threshold, within the time limit, then\n"
          "3. lifting the finger, within the time limit.\n") {
    CMockGestureManager gm;
    gm.addEdgeSwipeGesture(&SENSITIVITY);

    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
        // TODO Check progress == 0.5
        {wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {455, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

TEST_CASE("Edge Swipe: Timeout during swiping phase") {
    CMockGestureManager gm;
    gm.addEdgeSwipeGesture(&SENSITIVITY);

    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 150, 0, {300, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 520, 0, {600, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Edge Swipe: Timout during liftoff phase: \n"
          "1. touch down on edge of screen\n"
          "2. swipe more than the threshold, within the time limit, then\n"
          "3. do not lift finger until after timeout.") {
    CMockGestureManager gm;
    gm.addEdgeSwipeGesture(&SENSITIVITY);

    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
        {wf::touch::EVENT_TYPE_TOUCH_UP, 801, 0, {455, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE(
    "Edge Swipe: Fail check at the end for not starting swipe from an edge\n"
    "1. touch down somewhere NOT considered edge.\n"
    "2. swipe more than the threshold, within the time limit, then\n"
    "3. lift the finger, within the time limit.\n"
    "The starting position of the swipe is checked at the end and should "
    "fail.") {
    CMockGestureManager gm;
    gm.addEdgeSwipeGesture(&SENSITIVITY);

    const std::vector<TouchEvent> events{
        {wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {11, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        {wf::touch::EVENT_TYPE_MOTION, 200, 0, {461, 300}},
        // TODO Check progress == 0.5
        {wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {461, 300}},
    };
    const ExpectResult expected_result = {ExpectResultType::CHECK_PROGRESS,
                                          1.0};
    ProcessEvents(gm, expected_result, events);
}
