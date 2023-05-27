#include "Gestures.hpp"
#include <glm/glm.hpp>
#include <string>
#include <utility>

std::string stringifyDirection(gestureDirection direction) {
    std::string bind;
    if (direction & GESTURE_DIRECTION_LEFT) {
        bind += 'l';
    }

    if (direction & GESTURE_DIRECTION_RIGHT) {
        bind += 'r';
    }

    if (direction & GESTURE_DIRECTION_UP) {
        bind += 'u';
    }

    if (direction & GESTURE_DIRECTION_DOWN) {
        bind += 'd';
    }

    return bind;
}

std::string CompletedGesture::to_string() const {
    std::string bind = "";
    switch (type) {
        case GESTURE_TYPE_EDGE_SWIPE:
            bind += "edge";
            break;
        case GESTURE_TYPE_SWIPE:
            bind += "swipe";
            break;
        case GESTURE_TYPE_SWIPE_HOLD:
            // this gesture is only used internally for workspace swipe
            return "workspace_swipe";
    }

    if (type == GESTURE_TYPE_EDGE_SWIPE) {
        bind += stringifyDirection(this->edge_origin);
    } else {
        bind += std::to_string(finger_count);
    }

    bind += stringifyDirection(this->direction);
    return bind;
}

// The action is completed if any number of fingers is moved enough.
//
// This action should be followed by another that completes upon lifting a
// finger to achieve a gesture that completes after a multi-finger swipe is done
// and lifted.
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
        this->finger_count = state.fingers.size();
        for (auto& finger : state.fingers) {
            // TODO multiply tolerance by sensitivity?
            if (glm::length(finger.second.delta()) >
                GESTURE_INITIAL_TOLERANCE) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }

        return wf::touch::ACTION_STATUS_RUNNING;
    }

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

    if (state.get_center().get_drag_distance(target_direction) >=
        base_threshold / *sensitivity) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }
    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t
LiftoffAction::update_state(const wf::touch::gesture_state_t& state,
                            const wf::touch::gesture_event_t& event) {

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
bool IGestureManager::onTouchDown(const wf::touch::gesture_event_t& ev) {
    // NOTE @m_sGestureState is used in gesture-completed callbacks
    // during touch down it must be updated before updating the gestures
    // in touch up and motion, it must be updated AFTER updating the
    // gestures
    m_sGestureState.update(ev);
    updateGestures(ev);
    return false;
}

bool IGestureManager::onTouchUp(const wf::touch::gesture_event_t& ev) {
    updateGestures(ev);
    m_sGestureState.update(ev);
    return false;
}

bool IGestureManager::onTouchMove(const wf::touch::gesture_event_t& ev) {
    updateGestures(ev);
    m_sGestureState.update(ev);
    return false;
}

gestureDirection IGestureManager::find_swipe_edges(wf::touch::point_t point) {
    auto mon = getMonitorArea();

    gestureDirection edge_directions = 0;

    if (point.x <= mon.x + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_LEFT;
    }

    if (point.x >= mon.x + mon.w - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_RIGHT;
    }

    if (point.y <= mon.y + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_UP;
    }

    if (point.y >= mon.y + mon.h - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_DOWN;
    }

    return edge_directions;
}

void IGestureManager::addTouchGesture(
    std::unique_ptr<wf::touch::gesture_t> gesture) {
    m_vGestures.emplace_back(std::move(gesture));
}

// Multi fingered swipe, triggers once whenever you swipe more than the
// threshold.
void IGestureManager::addMultiFingerDragGesture(const float* sensitivity) {
    auto swipe = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE,
                                                sensitivity);
    // swipe->set_duration(GESTURE_BASE_DURATION * *sensitivity);

    // FIXME memory management be damned
    auto swipe_ptr = swipe.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(swipe));

    auto ack = [swipe_ptr, this]() {
        const auto gesture = CompletedGesture{GESTURE_TYPE_SWIPE_HOLD,
                                              swipe_ptr->target_direction,
                                              swipe_ptr->finger_count};
        this->handleGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    addTouchGesture(std::make_unique<wf::touch::gesture_t>(
        std::move(swipe_actions), ack, cancel));
}

// Multi fingered swipe, triggers on liftoff
//
// TODO rename
void IGestureManager::addMultiFingerSwipeThenLiftoffGesture(
    const float* sensitivity) {
    auto swipe = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE,
                                                sensitivity);
    swipe->set_duration(GESTURE_BASE_DURATION);
    auto swipe_liftoff = std::make_unique<LiftoffAction>();
    swipe_liftoff->set_duration(GESTURE_BASE_DURATION / 2);

    // FIXME memory management be damned
    auto swipe_ptr = swipe.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(swipe));
    swipe_actions.emplace_back(std::move(swipe_liftoff));

    auto ack = [swipe_ptr, this]() {
        const auto gesture =
            CompletedGesture{GESTURE_TYPE_SWIPE, swipe_ptr->target_direction,
                             swipe_ptr->finger_count};
        this->handleGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    addTouchGesture(std::make_unique<wf::touch::gesture_t>(
        std::move(swipe_actions), ack, cancel));
}

void IGestureManager::addEdgeSwipeGesture(const float* sensitivity) {
    // Edge swipe needs a quick release to be considered edge swipe
    auto edge = std::make_unique<CMultiAction>(MAX_SWIPE_DISTANCE, sensitivity);
    auto edge_release = std::make_unique<wf::touch::touch_action_t>(1, false);

    // FIXME make this adjustable:
    edge->set_duration(GESTURE_BASE_DURATION * *sensitivity);
    // TODO do I really need this:
    // edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * *sensitivity);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    // TODO make this adjustable:
    edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * *sensitivity);

    // FIXME proper memory management pls
    auto edge_ptr = edge.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>>
        edge_swipe_actions;
    edge_swipe_actions.emplace_back(std::move(edge));
    edge_swipe_actions.emplace_back(std::move(edge_release));

    auto ack = [edge_ptr, this]() {
        auto origin_edges =
            find_swipe_edges(m_sGestureState.get_center().origin);

        if (!origin_edges) {
            return;
        }
        auto direction = edge_ptr->target_direction;
        auto gesture   = CompletedGesture{GESTURE_TYPE_EDGE_SWIPE, direction,
                                        edge_ptr->finger_count, origin_edges};
        this->handleGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    auto gesture = std::make_unique<wf::touch::gesture_t>(
        std::move(edge_swipe_actions), ack, cancel);
    addTouchGesture(std::move(gesture));
}
