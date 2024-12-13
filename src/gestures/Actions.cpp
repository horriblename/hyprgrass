#include "Actions.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <wayfire/touch/touch.hpp>

wf::touch::action_status_t
CMultiAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->timeout) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        // cancel if previous fingers moved too much
        this->finger_count = state.fingers.size();
        for (auto& finger : state.fingers) {
            // TODO multiply tolerance by sensitivity?
            if (glm::length(finger.second.delta()) > GESTURE_INITIAL_TOLERANCE) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }

        return wf::touch::ACTION_STATUS_RUNNING;
    }

    if ((glm::length(state.get_center().delta()) >= MIN_SWIPE_DISTANCE) && (this->target_direction == 0)) {
        this->target_direction = state.get_center().get_direction();
    }

    if (this->target_direction == 0) {
        return wf::touch::ACTION_STATUS_RUNNING;
    }

    for (auto& finger : state.fingers) {
        if (finger.second.get_incorrect_drag_distance(this->target_direction) > this->get_move_tolerance()) {
            return wf::touch::ACTION_STATUS_CANCELLED;
        }
    }

    if (state.get_center().get_drag_distance(target_direction) >= base_threshold / *sensitivity) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }
    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
MultiFingerDownAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN && state.fingers.size() >= SEND_CANCEL_EVENT_FINGER_COUNT) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
MultiFingerTap::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->timeout) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    if (event.type == wf::touch::EVENT_TYPE_MOTION) {
        for (const auto& finger : state.fingers) {
            const auto delta = finger.second.delta();
            if (delta.x * delta.x + delta.y + delta.y > this->base_threshold / *this->sensitivity) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
LongPress::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->delay) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    switch (event.type) {
        case wf::touch::EVENT_TYPE_MOTION:
            for (const auto& finger : state.fingers) {
                const auto delta = finger.second.delta();
                if (delta.x * delta.x + delta.y + delta.y > this->base_threshold / *this->sensitivity) {
                    return wf::touch::ACTION_STATUS_CANCELLED;
                }
            }
            break;

        case wf::touch::EVENT_TYPE_TOUCH_DOWN:
            // TODO: also reset wl_timer here
            gesture_action_t::reset(event.time);
            this->update_external_timer_callback(event.time, *this->delay);
            break;

        case wf::touch::EVENT_TYPE_TOUCH_UP:
            return wf::touch::ACTION_STATUS_CANCELLED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
LiftoffAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
TouchUpOrDownAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP || event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
LiftAll::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP && state.fingers.size() == 0) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}
wf::touch::action_status_t
OnCompleteAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    auto status = this->action->update_state(state, event);

    if (status == wf::touch::ACTION_STATUS_COMPLETED) {
        this->callback(event.time);
    }

    return status;
}

wf::touch::action_status_t PinchAction::update_state(const wf::touch::gesture_state_t& state,
                                                     const wf::touch::gesture_event_t& event) {
    if (event.type != wf::touch::EVENT_TYPE_MOTION) {
        return wf::touch::ACTION_STATUS_RUNNING;
    }

    if (this->exceeds_tolerance(state)) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    const double current_scale = state.get_pinch_scale();
    const float lo             = std::clamp(1.0 - *this->threshold, 0.0, 1.0);
    float hi                   = 1.0 + *this->threshold;
    hi                         = hi < 1.0 ? 1.0 : hi;
    if (((current_scale < 1.0 && current_scale <= lo)) || (current_scale > 1.0 && current_scale >= hi)) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

bool PinchAction::exceeds_tolerance(const wf::touch::gesture_state_t& state) {
    return glm::length(state.get_center().delta()) > this->move_tolerance;
};
