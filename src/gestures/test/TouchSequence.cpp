#include "../TouchSequence.hpp"
#include "MockGestureManager.hpp"
#include <doctest/doctest.h>

TEST_CASE("layer namespaces are opt-in exact names") {
    CHECK_FALSE(matchesLayerNamespace("", "osk"));
    CHECK_FALSE(matchesLayerNamespace(" , \t, ", "osk"));
    CHECK_FALSE(matchesLayerNamespace("osk", ""));
    CHECK(matchesLayerNamespace("osk", "osk"));
    CHECK(matchesLayerNamespace("osk, waybar, control-panel", "waybar"));
    CHECK(matchesLayerNamespace(" , osk ,\tcontrol-panel\n,", "control-panel"));
    CHECK_FALSE(matchesLayerNamespace("osk", "OSK"));
    CHECK_FALSE(matchesLayerNamespace("osk", "my-osk"));
    CHECK_FALSE(matchesLayerNamespace("osk.*", "osk"));
}

TEST_CASE("only the first contact can exclude a sequence") {
    TouchSequenceBypass route;
    int device;
    CHECK_FALSE(route.down(0, &device, true, false));
    // A gesture started elsewhere stays a gesture when another finger hits a panel.
    CHECK_FALSE(route.down(1, &device, false, true));
    CHECK_FALSE(route.active());
    CHECK_FALSE(route.up(0));
    CHECK_FALSE(route.up(1));

    CHECK(route.down(0, &device, true, true));
    // Later contacts bypass even outside the panel or after the list is cleared.
    CHECK(route.down(1, &device, false, false));
    CHECK(route.up(0));
    CHECK(route.active());
    CHECK(route.down(2, &device, false, false));
    CHECK(route.up(1));
    CHECK(route.active());
    CHECK(route.up(2));
    CHECK_FALSE(route.active());
    CHECK_FALSE(route.down(0, &device, true, false));
}

TEST_CASE("unknown and repeated releases do not end another contact") {
    TouchSequenceBypass route;
    int device;
    REQUIRE(route.down(4, &device, true, true));
    CHECK(route.up(99));
    CHECK(route.active());
    CHECK(route.up(4));
    CHECK_FALSE(route.up(4));
    CHECK_FALSE(route.active());
    // Seat slots can be reused after a release.
    CHECK(route.down(4, &device, true, true));
    CHECK(route.up(4));
    CHECK_FALSE(route.active());
}

TEST_CASE("hardware cancel removes only its contact and permits slot reuse") {
    TouchSequenceBypass route;
    int device, otherDevice;
    REQUIRE(route.down(0, &device, true, true));
    REQUIRE(route.down(1, &device, false, false));
    route.cancel(0, &otherDevice);
    route.cancel(99, &device);
    CHECK(route.active());
    route.cancel(0, &device);
    CHECK(route.active());
    route.cancel(0, &device);
    CHECK(route.active());
    route.cancel(1, &device);
    CHECK_FALSE(route.active());
    CHECK_FALSE(route.up(1));
    CHECK_FALSE(route.down(0, &device, true, false));
    CHECK(route.down(0, &device, true, true));
    route.cancel(0, &device);
    CHECK_FALSE(route.active());
}

TEST_CASE("device removal preserves contacts on other devices") {
    TouchSequenceBypass route;
    int first, second, unrelated;
    REQUIRE(route.down(0, &first, true, true));
    REQUIRE(route.down(1, &first, false, false));
    REQUIRE(route.down(2, &second, false, false));
    route.removeDevice(&unrelated);
    route.removeDevice(&first);
    CHECK(route.active());
    route.cancel(2, &first);
    CHECK(route.active());
    CHECK(route.up(2));
    CHECK_FALSE(route.active());
    REQUIRE(route.down(0, &first, true, true));
    route.removeDevice(&first);
    CHECK_FALSE(route.active());
}

TEST_CASE("excluded holds never reach gesture recognition; later window drags still work") {
    TouchSequenceBypass route;
    int device;
    auto manager            = CMockGestureManager::newDragHandler();
    const float sensitivity = 1.0;
    const int64_t delay     = 400;
    manager.addLongPress(10, &sensitivity, &delay);

    using namespace wf::touch;
    const gesture_event_t down{EVENT_TYPE_TOUCH_DOWN, 0, 0, {200, 200}};
    const gesture_event_t hold{EVENT_TYPE_MOTION, 500, 0, {200, 200}};
    const gesture_event_t outside{EVENT_TYPE_MOTION, 600, 0, {900, 50}};
    const gesture_event_t up{EVENT_TYPE_TOUCH_UP, 700, 0, {900, 50}};

    if (!route.down(0, &device, true, true))
        manager.onTouchDown(down);
    for (const auto& motion : {hold, outside}) {
        if (!route.active())
            manager.onTouchMove(motion);
    }
    if (!route.up(0))
        manager.onTouchUp(up);
    CHECK_FALSE(manager.getActiveDragGesture().has_value());
    CHECK_FALSE(manager.sentWindowCancel);

    REQUIRE_FALSE(route.down(0, &device, true, false));
    manager.onTouchDown(down);
    manager.onTouchMove(hold);
    CHECK(manager.getActiveDragGesture().has_value());
    CHECK(manager.sentWindowCancel);
    manager.onTouchUp(up);
    CHECK(manager.dragEnded);
}
