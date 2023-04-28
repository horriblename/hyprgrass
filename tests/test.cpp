/**
 * If you can't already tell, I don't know much about C++ so I would like you to
 * divert your attention away from this atrocity. Thank you.
 *
 */
#include "MockGestureManager.hpp"
#include "tests.hpp"
#include "wayfire/touch/touch.hpp"
#include <cassert>
#include <memory>
#include <variant>

// @return true if passed
bool runTest(const TestCase& test) {
    auto mockGM = std::unique_ptr<CMockGestureManager>();

    for (auto& input : test.inputStream) {
        if (std::holds_alternative<wlr_touch_down_event>(input)) {
            auto ev = std::get<wlr_touch_down_event>(input);
            mockGM->onTouchDown(&ev);
        } else if (std::holds_alternative<wlr_touch_up_event>(input)) {
            auto ev = std::get<wlr_touch_up_event>(input);
            mockGM->onTouchUp(&ev);
        } else {
            auto ev = std::get<wlr_touch_motion_event>(input);
            mockGM->onTouchMove(&ev);
        }

        // TODO assert or something
    }

    return true;
}

int main() {
    auto test = TEST();
    runTest(test);
}
