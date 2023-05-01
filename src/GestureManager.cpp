#include "GestureManager.hpp"
#include "src/Compositor.hpp"
#include "src/debug/Log.hpp"
#include "src/managers/input/InputManager.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>

// constexpr double SWIPE_THRESHOLD = 30.;

std::string TouchGesture::to_string() const {
    std::string bind = "";
    switch (type) {
        case GESTURE_TYPE_EDGE_SWIPE:
            bind += "edge";
            break;
        case GESTURE_TYPE_SWIPE:
            bind += "swipe";
            break;
        case GESTURE_TYPE_NONE:
            return "";
            break;
    }

    bind += finger_count;
    bind += direction;
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
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(
        ev->touch->output_name ? ev->touch->output_name : "");

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    IGestureManager::onTouchDown(ev);

    return false;
}

bool CGestures::onTouchUp(wlr_touch_up_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    IGestureManager::onTouchUp(ev);

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    return false;
}

bool CGestures::onTouchMove(wlr_touch_motion_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    IGestureManager::onTouchMove(ev);

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeUpdate(ev->time_msec);
    }

    return false;
}

std::optional<SMonitorArea> CGestures::getMonitorArea() {
    if (!m_pLastTouchedMonitor) {
        Debug::log(ERR, "[touch-gestures] m_pLastTouchedMonitor is null!");
        return {};
    }
    auto& position = m_pLastTouchedMonitor->vecPosition;
    auto& geometry = m_pLastTouchedMonitor->vecSize;

    return SMonitorArea{position.x, position.y, geometry.x, geometry.y};
}
