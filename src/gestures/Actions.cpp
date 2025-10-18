#include "Actions.hpp"
#include <glm/geometric.hpp>
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

wf::touch::action_status_t
PinchAction::update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) {
    if (event.type != wf::touch::EVENT_TYPE_MOTION) {
        return wf::touch::ACTION_STATUS_RUNNING;
    }

    if (this->exceeds_tolerance(state)) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    const wf::touch::point_t center = state.get_center().origin;
    // can be negative in case of pinch out
    double delta_towards_center = 0.0;
    for (const auto& finger : state.fingers) {
        // how much the finger moved in the direction of the origin center,
        // a.k.a the scalar projection of finger.delta() on (center - finger.origin) 🤓
        //     (U dot V) / |V|
        // where U is the vector of finger.origin -> center.origin, and
        // V is the vector of finger.origin -> finger.current
        //
        // I'm not even 100% sure this is a good metric, libinput just checks that the first two
        // fingers each moved over the threshold. Could not find android source. Wayfire used "pinch
        // shrunk/expanded" over X% as a threshold, which honestly did not feel good.
        //
        // TODO: cancel if the finger is moving too much perpendicular to the center
        auto u = center - finger.second.origin;
        auto v = finger.second.delta();
        delta_towards_center += glm::dot(u, v) / glm::length(v);
    }
    delta_towards_center /= state.fingers.size();

    if (glm::length(delta_towards_center) >= PINCH_INCORRECT_DRAG_TOLERANCE) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

bool PinchAction::exceeds_tolerance(const wf::touch::gesture_state_t& state) {
    return glm::length(state.get_center().delta()) > this->move_tolerance;
};
