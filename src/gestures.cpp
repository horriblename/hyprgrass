#include "gestures.hpp"
#include "src/Compositor.hpp"
#include "src/debug/Log.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
#include <wayfire/touch/touch.hpp>

constexpr double SWIPE_THRESHOLD = 30.;

constexpr double TEMP_CONFIG_SENSITIVITY = 1;

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

CGestures::CGestures() {
    addDefaultGestures();
}

void CGestures::addDefaultGestures() {
    static const double SENSITIVITY = TEMP_CONFIG_SENSITIVITY;

    auto swipe =
        std::make_unique<CMultiAction>(0.75 * MAX_SWIPE_DISTANCE / SENSITIVITY);
    swipe->set_duration(GESTURE_BASE_DURATION * SENSITIVITY);
    swipe->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * SENSITIVITY);

    // Edge swipe needs a quick release to be considered edge swipe
    auto edge =
        std::make_unique<CMultiAction>(MAX_SWIPE_DISTANCE / SENSITIVITY);
    auto edge_release = std::make_unique<wf::touch::touch_action_t>(1, false);
    edge->set_duration(GESTURE_BASE_DURATION * SENSITIVITY);
    edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * SENSITIVITY);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * SENSITIVITY);

    // HACK I should figure out how to properly manage memory here
    auto swipe_ptr = swipe.get();
    auto edge_ptr  = edge.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions,
        edge_swipe_actions;
    swipe_actions.emplace_back(std::move(swipe));
    edge_swipe_actions.emplace_back(std::move(edge));
    edge_swipe_actions.emplace_back(std::move(edge_release));

    auto ack_swipe = [swipe_ptr, this]() {
        if (!swipe_ptr) {
            // Not sure if swipe_ptr is actually usable - see above
            Debug::log(ERR, "swipe pointer missing!");
            return;
        }
        gestureDirection possible_edges =
            find_swipe_edges(m_pGestureState.get_center().origin);
        if (possible_edges)
            return;

        gestureDirection direction = swipe_ptr->target_direction;
        auto gesture               = TouchGesture{GESTURE_TYPE_SWIPE, direction,
                                    swipe_ptr->finger_count};
        handleGesture(gesture);
    };

    auto ack_edge_swipe = [edge_ptr, this]() {
        if (!edge_ptr) {
            // Not sure if edge_ptr is actually usable - see above
            Debug::log(ERR, "edge swipe pointer missing!");
            return;
        }
        gestureDirection possible_edges =
            find_swipe_edges(m_pGestureState.get_center().origin);
        // FIXME target_direction was never assigned a meaningful value
        gestureDirection target_dir = edge_ptr->target_direction;

        possible_edges &= target_dir;
        if (possible_edges) {
            auto gesture = TouchGesture{GESTURE_TYPE_EDGE_SWIPE, target_dir,
                                        edge_ptr->finger_count};
            handleGesture(gesture);
        }
    };
    auto multi_swipe = std::make_unique<wf::touch::gesture_t>(
        std::move(swipe_actions), ack_swipe);
    auto edge_swipe = std::make_unique<wf::touch::gesture_t>(
        std::move(edge_swipe_actions), ack_edge_swipe);

    addTouchGesture(std::move(multi_swipe));
    addTouchGesture(std::move(edge_swipe));

    Debug::log(INFO, "initialized a CGestures object");
}

void CGestures::emulateSwipeBegin() {}
void CGestures::emulateSwipeEnd() {}

void CGestures::addTouchGesture(std::unique_ptr<wf::touch::gesture_t> gesture) {
    m_pGestures.emplace_back(std::move(gesture));
}

void CGestures::handleGesture(const TouchGesture& gev) {
    Debug::log(INFO, "handling gesture {direction = %d, fingers = %d }",
               gev.direction, gev.finger_count);
}

void CGestures::updateGestures(const wf::touch::gesture_event_t& ev) {
    for (auto& gesture : m_pGestures) {
        if (m_pGestureState.fingers.size() == 1 &&
            ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
            gesture->reset(ev.time);
        }

        gesture->update_state(ev);
    }
}

// @return whether or not to inhibit further actions
bool CGestures::onTouchDown(wlr_touch_down_event* ev) {
    m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(
        ev->touch->output_name ? ev->touch->output_name : "");

    // if (ev->touch_id == 0) {
    //     return false;
    // }

    wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_DOWN,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {ev->x, ev->y}};

    m_pGestureState.update(gesture_event);

    // was supposed to refoces here?

    updateGestures(gesture_event);

    return false;
}

bool CGestures::onTouchUp(wlr_touch_up_event* ev) {
    const auto lift_off_pos = m_pGestureState.fingers[ev->touch_id].current;

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_UP,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {lift_off_pos.x, lift_off_pos.y},
    };

    updateGestures(gesture_event);
    m_pGestureState.update(gesture_event);
    return false;
}

bool CGestures::onTouchMove(wlr_touch_motion_event* ev) {
    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {ev->x, ev->y},
    };
    updateGestures(gesture_event);
    m_pGestureState.update(gesture_event);

    return false;
}

uint32_t CGestures::find_swipe_edges(wf::touch::point_t point) {
    if (!m_pLastTouchedMonitor) {
        Debug::log(ERR, "[touch-gestures] m_pLastTouchedMonitor is null!");
        return 0;
    }
    auto position = m_pLastTouchedMonitor->vecPosition;
    auto geometry = m_pLastTouchedMonitor->vecSize;

    gestureDirection edge_directions = 0;

    if (point.x <= position.x + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_RIGHT;
    }

    if (point.x >= position.x + geometry.x - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_LEFT;
    }

    if (point.y <= position.y + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_DOWN;
    }

    if (point.y >= position.y + geometry.y - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_UP;
    }

    return edge_directions;
}
