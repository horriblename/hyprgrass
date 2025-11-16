#include <iostream>
#include <variant>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "MockGestureManager.hpp"
#include "wayfire/touch/touch.hpp"
#include <vector>

constexpr float SENSITIVITY        = 1.0;
constexpr int64_t LONG_PRESS_DELAY = GESTURE_BASE_DURATION;
constexpr long int EDGE_MARGIN     = 10;

static void log_start_of_test() {
    std::cout << "=== start of: " << doctest::detail::g_cs->currentTest->m_name << std::endl;
}

enum class ExpectResultType {
    COMPLETED,
    DRAG_TRIGGERED,
    DRAG_ENDED,
    CANCELLED,
    CHECK_PROGRESS,
    NOTHING_HAPPENED,
};

struct ExpectResult {
    ExpectResultType type;

    // variables below only used when type == CHECK_PROGRESS
    float progress = 0.0;

    // which of the m_vGestures to use
    // usually the first (0) in tests
    int gesture_index = 0;
};

using Ev         = wf::touch::gesture_event_t;
using TouchEvent = std::variant<Ev, ExpectResult>;
using wf::touch::point_t;

void checkCondition(CMockGestureManager& gm, ExpectResult expect) {
    switch (expect.type) {
        case ExpectResultType::COMPLETED:
            CHECK(gm.triggered);
            break;
        case ExpectResultType::DRAG_TRIGGERED:
            CHECK(gm.getActiveDragGesture().has_value());
            CHECK(gm.eventForwardingInhibited());
            break;
        case ExpectResultType::DRAG_ENDED:
            CHECK(gm.dragEnded);
            CHECK(gm.eventForwardingInhibited());
            break;
        case ExpectResultType::CANCELLED:
            CHECK(gm.cancelled);
            break;
        case ExpectResultType::NOTHING_HAPPENED:
            CHECK(!gm.cancelled);
            CHECK(!gm.dragEnded);
            CHECK(!gm.getActiveDragGesture().has_value());
            break;
        case ExpectResultType::CHECK_PROGRESS:
            const auto got = gm.getGestureAt(expect.gesture_index)->get()->get_progress();
            // fuck floating point math
            CHECK(std::abs(got - expect.progress) < 1e-5);
            break;
    }
}

// NOTE each event implicitly means the gesture has not been triggered/cancelled
// yet
void ProcessEvent(CMockGestureManager& gm, const wf::touch::gesture_event_t& ev) {
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

void ProcessEvents(CMockGestureManager& gm, ExpectResult expect, const std::vector<TouchEvent>& events) {
    for (const auto& ev : events) {
        if (std::holds_alternative<Ev>(ev)) {
            ProcessEvent(gm, std::get<Ev>(ev));
        } else {
            checkCondition(gm, std::get<ExpectResult>(ev));
        }
    }

    checkCondition(gm, expect);
}

TEST_CASE(
    "Multifinger: block touch events to client surfaces when more than a "
    "certain number of fingers touch down." *
    // multi-finger used to cancel touch events on 3 finger touch down
    // currently removed but may bring it back
    doctest::should_fail()
) {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addMultiFingerGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 120, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 140, 2, {550, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CHECK_PROGRESS, .progress = 1.0 / 3.0}, events);

    CHECK(gm.eventForwardingInhibited());
}

TEST_CASE("Swipe Drag: Start drag upon moving more than the threshold") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addMultiFingerGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {0, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::DRAG_TRIGGERED}, events);
}

TEST_CASE("Swipe Drag: Cancel 3 finger swipe due to moving too much before "
          "adding new finger, but not enough to trigger 3 finger swipe first") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addMultiFingerGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {401, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 110, 0, {409, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 110, 1, {459, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 120, 3, {600, 280}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Swipe: Complete upon moving more than the threshold then lifting a "
          "finger") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addMultiFingerGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {0, 290}},
        // TODO CHECK progress == 0.5
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {50, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {100, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {100, 290}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

TEST_CASE("Multi-finger Tap") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addMultiFingerTap(SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 120, 2, {550, 290}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

TEST_CASE("Multi-finger Tap: Timeout") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addMultiFingerTap(SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 510, 2, {550, 290}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Multi-finger Tap: finger moved too much") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addMultiFingerTap(SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}}, Ev{wf::touch::EVENT_TYPE_MOTION, 120, 1, {650, 290}},
        // {wf::touch::EVENT_TYPE_TOUCH_UP, 130, 2, {550, 290}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Long press: begin drag") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addLongPress(SWIPE_THRESHOLD, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {460, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 300, 1, {510, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 511, 2, {560, 300}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::DRAG_TRIGGERED}, events);
}

TEST_CASE("Long press and drag: drag ends after all fingers lifted") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addLongPress(SWIPE_THRESHOLD, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {460, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 300, 1, {510, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 511, 2, {560, 300}},
        ExpectResult{ExpectResultType::DRAG_TRIGGERED},
        Ev{wf::touch::EVENT_TYPE_MOTION, 530, 0, {470, 310}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 550, 2, {560, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 550, 0, {560, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 550, 1, {560, 300}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::DRAG_ENDED}, events);
}

TEST_CASE("Long press and drag: touch down does nothing") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addLongPress(SWIPE_THRESHOLD, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {460, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 300, 1, {510, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 511, 2, {560, 300}},
        ExpectResult{ExpectResultType::DRAG_TRIGGERED},
        Ev{wf::touch::EVENT_TYPE_MOTION, 530, 0, {470, 310}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 550, 3, {560, 300}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::CHECK_PROGRESS, .progress = 0.5}, events);
}

TEST_CASE("Long press and drag: cancelled due to short hold duration") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addLongPress(SWIPE_THRESHOLD, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {460, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 300, 1, {510, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 350, 2, {560, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 400, 1, {510, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 500, 2, {560, 300}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Long press and drag: cancelled due to too much movement") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addLongPress(SWIPE_THRESHOLD, &SENSITIVITY, &LONG_PRESS_DELAY);

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {450, 290}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 105, 1, {500, 300}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 110, 2, {550, 290}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {650, 290}},
    };

    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Edge Swipe: Complete upon: \n"
          "1. touch down on edge of screen\n"
          "2. swiping more than the threshold, within the time limit, then\n"
          "3. lifting the finger, within the time limit.\n") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
        // TODO Check progress == 0.5
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {455, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);
}

// haven't gotten around to checking what's wrong
TEST_CASE("Edge Swipe: Timeout during swiping phase") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {154, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 551, 0, {600, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CANCELLED}, events);
}

TEST_CASE("Edge Swipe: Fail check at the end for not starting swipe from an edge\n"
          "1. touch down somewhere NOT considered edge.\n"
          "2. swipe more than the threshold, within the time limit, then\n"
          "3. lift the finger, within the time limit.\n"
          "The starting position of the swipe is checked at the end and should "
          "fail.") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {11, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {461, 300}},
        // TODO Check progress == 0.5
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {461, 300}},
    };
    const ExpectResult expected_result = {ExpectResultType::CHECK_PROGRESS, 1.0};
    ProcessEvents(gm, expected_result, events);
}

TEST_CASE("Edge Swipe Drag: begin") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
    };
    const ExpectResult expected_result = {ExpectResultType::DRAG_TRIGGERED, 1.0};
    ProcessEvents(gm, expected_result, events);
}

TEST_CASE("Edge Swipe Drag: emits drag end event") {
    auto gm = CMockGestureManager::newDragHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );

    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {5, 300}}, Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
        ExpectResult{ExpectResultType::DRAG_TRIGGERED},         Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 250, 0, {600, 300}},   Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {700, 400}},
    };

    const ExpectResult expected_result = {ExpectResultType::DRAG_ENDED, 1.0};
    ProcessEvents(gm, expected_result, events);
}

TEST_CASE("Edge Swipe: margins") {
    SUBCASE("custom margin: less than threshold triggers") {
        auto gm     = CMockGestureManager::newDragHandler();
        long margin = 20;
        gm.addEdgeSwipeGesture(
            SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &margin
        );

        const std::vector<TouchEvent> events{
            Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {19, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
            ExpectResult{ExpectResultType::DRAG_TRIGGERED},
            Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 250, 0, {600, 300}},
            Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {700, 400}},
        };

        const ExpectResult expected_result = {ExpectResultType::DRAG_ENDED, 1.0};
        ProcessEvents(gm, expected_result, events);
    }

    SUBCASE("with non-zero offset") {
        auto gm       = CMockGestureManager::newDragHandler();
        gm.mon_offset = {2000, 0};
        long margin   = 20;
        gm.addEdgeSwipeGesture(
            SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &margin
        );

        const std::vector<TouchEvent> events{
            Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {2019, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
            ExpectResult{ExpectResultType::DRAG_TRIGGERED},
            Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {2455, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 250, 0, {600, 300}},
            Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {2700, 400}},
        };

        const ExpectResult expected_result = {ExpectResultType::DRAG_ENDED, 1.0};
        ProcessEvents(gm, expected_result, events);
    }

    SUBCASE("custom margin: more than threshold does not trigger") {
        auto gm     = CMockGestureManager::newDragHandler();
        long margin = 20;
        gm.addEdgeSwipeGesture(
            SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &margin
        );

        const std::vector<TouchEvent> events{
            Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {21, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 150, 0, {250, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
            Ev{wf::touch::EVENT_TYPE_MOTION, 250, 0, {600, 300}},
            Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {700, 400}},
        };

        const ExpectResult expected_result = {ExpectResultType::NOTHING_HAPPENED, 1.0};
        ProcessEvents(gm, expected_result, events);
    }
}

TEST_CASE("Edge swipe: block touch events") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addEdgeSwipeGesture(
        SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY, &EDGE_MARGIN
    );
    const std::vector<TouchEvent> events{
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {10, 300}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {455, 300}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::CHECK_PROGRESS, .progress = 1.0 / 2.0}, events);

    CHECK(gm.eventForwardingInhibited());
}

TEST_CASE("Pinch in: full drag") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addPinchGesture(PINCH_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        // origin center is (200, 180)
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {150, 200}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {200, 140}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {350, 200}},

        // pinch outwards without passing the threshold
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {110, 200}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 90}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {290, 200}},

        // pinch in
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {140, 190}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 150}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {260, 190}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 0, {170, 185}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 1, {180, 160}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 2, {250, 190}},
        ExpectResult{ExpectResultType::DRAG_TRIGGERED},

        // continue drag
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 0, {160, 185}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 1, {150, 160}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 2, {230, 190}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {140, 190}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::DRAG_ENDED}, events);

    CHECK(gm.eventForwardingInhibited());
}

TEST_CASE("Pinch out: full drag") {
    log_start_of_test();
    auto gm = CMockGestureManager::newDragHandler();
    gm.addPinchGesture(PINCH_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        // origin center is (200, 180)
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {150, 200}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {200, 140}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {250, 200}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {110, 200}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 90}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {290, 200}},

        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {60, 210}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 120}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {340, 210}},
        ExpectResult{ExpectResultType::DRAG_TRIGGERED},

        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 0, {30, 250}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 1, {200, 20}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 2, {400, 220}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {50, 200}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::DRAG_ENDED}, events);

    CHECK(gm.eventForwardingInhibited());
}

TEST_CASE("Pinch out: completed gesture") {
    log_start_of_test();
    auto gm = CMockGestureManager::newCompletedGestureOnlyHandler();
    gm.addPinchGesture(PINCH_INCORRECT_DRAG_TOLERANCE, &SENSITIVITY, &LONG_PRESS_DELAY);
    const std::vector<TouchEvent> events{
        // origin center is (200, 180)
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 0, {150, 200}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 1, {200, 140}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_DOWN, 100, 2, {250, 200}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {110, 200}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 90}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {290, 200}},

        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 0, {60, 210}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 1, {200, 120}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 200, 2, {340, 210}},

        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 0, {30, 250}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 1, {200, 20}},
        Ev{wf::touch::EVENT_TYPE_MOTION, 260, 2, {400, 220}},
        Ev{wf::touch::EVENT_TYPE_TOUCH_UP, 300, 0, {50, 200}},
    };
    ProcessEvents(gm, {.type = ExpectResultType::COMPLETED}, events);

    CHECK(gm.eventForwardingInhibited());
}
