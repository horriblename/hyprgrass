#include "GestureManager.hpp"
#include "src/Compositor.hpp"
#include "src/debug/Log.hpp"
#include "src/managers/input/InputManager.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>

// constexpr double SWIPE_THRESHOLD = 30.;

CGestures::CGestures() {
    static auto* const PSENSITIVITY =
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "plugin:touch_gestures:sensitivity")
             ->floatValue;
    static auto* const PTOUCHSWIPEFINGERS =
        &HyprlandAPI::getConfigValue(
             PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers")
             ->intValue;

    // FIXME time arg of @emulateSwipeBegin should probably be assigned
    // something useful (though its not really used later)
    auto workspaceSwipeBegin = [this]() { this->emulateSwipeBegin(0); };
    // TODO make sensitivity and workspace_swipe_fingers dynamic
    addTouchGesture(newWorkspaceSwipeStartGesture(
        *PSENSITIVITY, *PTOUCHSWIPEFINGERS, workspaceSwipeBegin, []() {}));
}

void CGestures::emulateSwipeBegin(uint32_t time) {
    static auto* const PSWIPEFINGERS =
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "gestures:workspace_swipe_fingers")
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
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "gestures:workspace_swipe_distance")
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

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    IGestureManager::onTouchDown(ev);

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
