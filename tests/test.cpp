#include "../src/Gestures.hpp"
#include "tests.hpp"
#include "wayfire/touch/touch.hpp"
#include <cassert>
#include <memory>
#include <variant>

void simulateTouchDown(wlr_touch_down_event& ev) {
    wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_DOWN,
        .time   = ev.time_msec,
        .finger = ev.touch_id,
        .pos    = {ev->x, ev->y}};
}

// @return true if passed
bool runTest(const TestCase& test) {
    for (auto& input : test.inputStream) {
        if (std::holds_alternative<wlr_touch_down_event>(input)) {
            auto ev = std::get<wlr_touch_down_event>(input);
            simulateTouchDown(input);
        } else if (std::holds_alternative<wlr_touch_up_event>(input)) {
            auto ev = std::get<wlr_touch_up_event>(input);
            // TODO
        } else {
            auto ev = std::get<wlr_touch_motion_event>(input);
            // TODO
        }

        // TODO assert or something
    }

    return true;
}

int main() {
    runTest(TEST());
}
