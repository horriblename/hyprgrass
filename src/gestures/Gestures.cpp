#include "Gestures.hpp"
#include <glm/glm.hpp>

// The action is completed if any number of fingers is moved enough,
// and can be later cancelled if a new finger touches down
wf::touch::action_status_t
CMultiAction::update_state(const wf::touch::gesture_state_t& state,
                           const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        // cancel if previous fingers moved too much
        finger_count = state.fingers.size();
        for (auto& finger : state.fingers) {
            if (glm::length(finger.second.delta()) >
                GESTURE_INITIAL_TOLERANCE) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }

        return wf::touch::ACTION_STATUS_RUNNING;
    }

    // swipe case

    if ((glm::length(state.get_center().delta()) >= MIN_SWIPE_DISTANCE) &&
        (this->target_direction == 0)) {
        this->target_direction = state.get_center().get_direction();
    }

    if (this->target_direction == 0) {
        return wf::touch::ACTION_STATUS_RUNNING;
    }

    for (auto& finger : state.fingers) {
        if (finger.second.get_incorrect_drag_distance(this->target_direction) >
            this->get_move_tolerance()) {
            return wf::touch::ACTION_STATUS_CANCELLED;
        }
    }

    if (state.get_center().get_drag_distance(target_direction) >= threshold) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }
    return wf::touch::ACTION_STATUS_RUNNING;
}
// FIXME move this into CGestures / abstract class coz its bad
//
// Create a new Gesture that triggers when @fingers amount of fingers touch
// down on the screen. For the purpose of workspace swipe, cancelling the
// swipe must be handled separately when a finger is lifted or when another
// finger is added
std::unique_ptr<wf::touch::gesture_t>
newWorkspaceSwipeStartGesture(const double sensitivity, const int fingers,
                              wf::touch::gesture_callback_t completed_cb,
                              wf::touch::gesture_callback_t cancel_cb) {
    auto swipe = std::make_unique<wf::touch::touch_action_t>(fingers, true);
    swipe->set_duration(GESTURE_BASE_DURATION * sensitivity);

    // auto swipe_ptr = swipe.get();
    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(swipe));

    return std::make_unique<wf::touch::gesture_t>(std::move(swipe_actions),
                                                  completed_cb, cancel_cb);
}

void IGestureManager::updateGestures(const wf::touch::gesture_event_t& ev) {
    for (auto& gesture : m_vGestures) {
        if (m_sGestureState.fingers.size() == 1 &&
            ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
            gesture->reset(ev.time);
        }

        gesture->update_state(ev);
    }
}

// @return whether or not to inhibit further actions
bool IGestureManager::onTouchDown(wlr_touch_down_event* ev) {
    wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_DOWN,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {ev->x, ev->y}};

    m_sGestureState.update(gesture_event);

    updateGestures(gesture_event);

    return false;
}

bool IGestureManager::onTouchUp(wlr_touch_up_event* ev) {
    const auto lift_off_pos = m_sGestureState.fingers[ev->touch_id].current;

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_UP,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {lift_off_pos.x, lift_off_pos.y},
    };

    m_sGestureState.update(gesture_event);
    updateGestures(gesture_event);
    return false;
}

bool IGestureManager::onTouchMove(wlr_touch_motion_event* ev) {
    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {ev->x, ev->y},
    };

    m_sGestureState.update(gesture_event);
    updateGestures(gesture_event);

    return false;
}

void IGestureManager::addTouchGesture(
    std::unique_ptr<wf::touch::gesture_t> gesture) {
    m_vGestures.emplace_back(std::move(gesture));
}
