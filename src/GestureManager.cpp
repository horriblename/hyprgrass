#include "GestureManager.hpp"
#include "src/Compositor.hpp"
#include "src/debug/Log.hpp"
#include "src/managers/input/InputManager.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>

constexpr double SWIPE_THRESHOLD = 30.;

// wayfire allows values between 0.1 ~ 5
constexpr double TEMP_CONFIG_SENSITIVITY          = 1;
constexpr int TEMP_CONFIG_WORKSPACE_SWIPE_FINGERS = 3;
constexpr double TEMP_CONFIG_HOLD_DELAY           = 500;

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

CGestures::CGestures() {
    addDefaultGestures();
    // FIXME time arg of @emulateSwipeBegin should probably be assigned
    // something useful (though its not really used later)
    auto workspaceSwipeBegin = [this]() { this->emulateSwipeBegin(0); };
    // auto workspaceSwipeEnd   = [this]() { this->emulateSwipeEnd(); };
    addTouchGesture(newWorkspaceSwipeStartGesture(
        TEMP_CONFIG_SENSITIVITY, TEMP_CONFIG_WORKSPACE_SWIPE_FINGERS,
        workspaceSwipeBegin, []() {}));
}

// NOTE should deprecate this
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
            Debug::log(ERR, "[touch-gesture] swipe pointer missing!");
            return;
        }
        gestureDirection possible_edges =
            find_swipe_edges(m_sGestureState.get_center().origin);
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
            Debug::log(ERR, "[touch-gesture] edge swipe pointer missing!");
            return;
        }
        gestureDirection possible_edges =
            find_swipe_edges(m_sGestureState.get_center().origin);
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
}

void CGestures::emulateSwipeBegin(uint32_t time) {
    static auto* const PSWIPEFINGERS =
        &g_pConfigManager->getConfigValuePtr("gestures:workspace_swipe_fingers")
             ->intValue;

    // HACK .pointer is not used by g_pInputManager->onSwipeBegin so it's fine I
    // think
    auto emulated_swipe =
        wlr_pointer_swipe_begin_event{.pointer   = nullptr,
                                      .time_msec = time,
                                      .fingers   = (uint32_t)*PSWIPEFINGERS};
    g_pInputManager->onSwipeBegin(&emulated_swipe);

    m_vGestureLastCenter    = m_sGestureState.get_center().current;
    m_bWorkspaceSwipeActive = true;
}

void CGestures::emulateSwipeEnd(uint32_t time, bool cancelled) {
    auto emulated_swipe = wlr_pointer_swipe_end_event{
        .pointer = nullptr, .time_msec = time, .cancelled = cancelled};
    g_pInputManager->onSwipeEnd(&emulated_swipe);

    m_bWorkspaceSwipeActive = false;
}

void CGestures::emulateSwipeUpdate(uint32_t time) {
    static auto* const PSWIPEDIST =
        &g_pConfigManager
             ->getConfigValuePtr("gestures:workspace_swipe_distance")
             ->intValue;

    if (!g_pInputManager->m_sActiveSwipe.pMonitor) {
        Debug::log(
            ERR, "ignoring touch gesture motion event due to missing monitor!");
        return;
    }

    auto currentCenter = m_sGestureState.get_center().current;

    // touch coords are within 0 to 1, we need to scale it with PSWIPEDIST for
    // one to one gesture
    auto emulated_swipe = wlr_pointer_swipe_update_event{
        .pointer   = nullptr,
        .time_msec = time,
        .fingers   = (uint32_t)m_sGestureState.fingers.size(),
        .dx        = (currentCenter.x - m_vGestureLastCenter.x) * *PSWIPEDIST,
        .dy        = (currentCenter.y - m_vGestureLastCenter.y) * *PSWIPEDIST};

    g_pInputManager->onSwipeUpdate(&emulated_swipe);
    m_vGestureLastCenter = currentCenter;
}

void CGestures::handleGesture(const TouchGesture& gev) {
    Debug::log(
        INFO,
        "[touch-gestures] handling gesture {direction = %d, fingers = %d }",
        gev.direction, gev.finger_count);
}

// @return whether or not to inhibit further actions
bool CGestures::onTouchDown(wlr_touch_down_event* ev) {
    m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(
        ev->touch->output_name ? ev->touch->output_name : "");

    IGestureManager::onTouchDown(ev);

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    return false;
}

bool CGestures::onTouchUp(wlr_touch_up_event* ev) {
    IGestureManager::onTouchUp(ev);

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    return false;
}

bool CGestures::onTouchMove(wlr_touch_motion_event* ev) {
    IGestureManager::onTouchMove(ev);

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeUpdate(ev->time_msec);
    }

    return false;
}

// swiping from left edge will result in GESTURE_DIRECTION_RIGHT etc.
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
