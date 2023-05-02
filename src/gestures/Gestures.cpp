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

std::unique_ptr<wf::touch::gesture_t>
newEdgeSwipeGesture(const double sensitivity, edge_swipe_callback completed_cb,
                    edge_swipe_callback cancel_cb) {

    // Edge swipe needs a quick release to be considered edge swipe
    auto edge =
        std::make_unique<CMultiAction>(MAX_SWIPE_DISTANCE / sensitivity);
    auto edge_release = std::make_unique<wf::touch::touch_action_t>(1, false);
    edge->set_duration(GESTURE_BASE_DURATION * sensitivity);
    edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * sensitivity);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * sensitivity);

    // FIXME proper memory management pls
    auto edge_ptr = edge.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>>
        edge_swipe_actions;
    edge_swipe_actions.emplace_back(std::move(edge));
    edge_swipe_actions.emplace_back(std::move(edge_release));

    // TODO this is so convoluted i hate it
    auto ack    = [edge_ptr, completed_cb]() { completed_cb(edge_ptr); };
    auto cancel = [edge_ptr, cancel_cb]() { cancel_cb(edge_ptr); };

    return std::make_unique<wf::touch::gesture_t>(std::move(edge_swipe_actions),
                                                  ack, cancel);
}

void IGestureManager::updateGestures(const wf::touch::gesture_event_t& ev) {
    m_sGestureState.update(ev);
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
    updateGestures(ev);
    return false;
}

bool IGestureManager::onTouchUp(const wf::touch::gesture_event_t& ev) {
    updateGestures(ev);
    return false;
}

bool IGestureManager::onTouchMove(const wf::touch::gesture_event_t& ev) {
    updateGestures(ev);
    return false;
}

// swiping from left edge will result in GESTURE_DIRECTION_RIGHT etc.
gestureDirection IGestureManager::find_swipe_edges(wf::touch::point_t point) {
    auto mon = getMonitorArea();

    gestureDirection edge_directions = 0;

    if (point.x <= mon.x + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_RIGHT;
    }

    if (point.x >= mon.x + mon.w - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_LEFT;
    }

    if (point.y <= mon.y + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_DOWN;
    }

    if (point.y >= mon.y + mon.h - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_UP;
    }

    return edge_directions;
}

void IGestureManager::addTouchGesture(
    std::unique_ptr<wf::touch::gesture_t> gesture) {
    m_vGestures.emplace_back(std::move(gesture));
}
