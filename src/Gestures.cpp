#include "Gestures.hpp"

std::unique_ptr<wf::touch::gesture_t>
newWorkspaceSwipeStartGesture(const double sensitivity) {
    auto swipe = std::make_unique<wf::touch::touch_action_t>(3, true);
    swipe->set_duration(GESTURE_BASE_DURATION * sensitivity);

    auto swipe_ptr = swipe.get();
    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(swipe));

    auto ack_swipe = [swipe_ptr]() {
        // gestureDirection possible_edges =
        //     find_swipe_edges(m_pGestureState.get_center().origin);
        // if (possible_edges)
        //     return;

        // start swipe
        // cout << "workspace swipe started";
    };
    auto cancel_swipe = [swipe_ptr]() {
        // end swipe
        // cout << "workspace swipe ended";
    };

    return std::make_unique<wf::touch::gesture_t>(std::move(swipe_actions),
                                                  ack_swipe, cancel_swipe);
}
