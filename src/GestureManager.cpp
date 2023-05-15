#include "GestureManager.hpp"
#include <algorithm>
#include <cstdint>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
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

    addEdgeSwipeGesture(PSENSITIVITY);
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
        .dx = (currentCenter.x - m_vGestureLastCenter.x) / m_sMonitorArea.w *
              *PSWIPEDIST,
        .dy = (currentCenter.y - m_vGestureLastCenter.y) / m_sMonitorArea.h *
              *PSWIPEDIST};

    g_pInputManager->onSwipeUpdate(&emulated_swipe);
    m_vGestureLastCenter = currentCenter;
}

void CGestures::handleGesture(const TouchGesture& gev) {
    auto bind = gev.to_string();
    Debug::log(LOG, "[touch-gesture] Gesture Triggered: %s", bind.c_str());

    for (const auto& k : g_pKeybindManager->m_lKeybinds) {
        if (k.key != bind)
            continue;

        const auto DISPATCHER =
            g_pKeybindManager->m_mDispatchers.find(k.handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end()) {
            Debug::log(
                ERR,
                "Invalid handler in a keybind! (handler %s does not exist)",
                k.handler.c_str());
            continue;
        }

        // call the dispatcher
        Debug::log(LOG, "[touch-gesture] calling dispatcher (%s)",
                   bind.c_str());

        if (k.handler == "pass")
            continue;

        DISPATCHER->second(k.arg);
        m_bDispatcherFound = true;
    }
}

// @return whether or not to inhibit further actions
bool CGestures::onTouchDown(wlr_touch_down_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(
        ev->touch->output_name ? ev->touch->output_name : "");

    const auto PDEVIT = std::find_if(
        g_pInputManager->m_lTouchDevices.begin(),
        g_pInputManager->m_lTouchDevices.end(), [&](const STouchDevice& other) {
            return other.pWlrDevice == &ev->touch->base;
        });

    if (PDEVIT != g_pInputManager->m_lTouchDevices.end() &&
        !PDEVIT->boundOutput.empty()) {
        m_pLastTouchedMonitor =
            g_pCompositor->getMonitorFromName(PDEVIT->boundOutput);
    }
    m_pLastTouchedMonitor = m_pLastTouchedMonitor
                                ? m_pLastTouchedMonitor
                                : g_pCompositor->m_pLastMonitor;

    const auto& position = m_pLastTouchedMonitor->vecPosition;
    const auto& geometry = m_pLastTouchedMonitor->vecSize;
    m_sMonitorArea       = {position.x, position.y, geometry.x, geometry.y};

    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    // NOTE @wlr_touch_down_event.x and y uses a number between 0 and 1 to
    // represent "how many percent of screen" whereas
    // @wf::touch::gesture_event_t uses PIXELS as unit
    auto pos = wlrTouchEventPositionAsPixels(ev->x, ev->y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_DOWN,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = pos,
    };

    IGestureManager::onTouchDown(gesture_event);

    // TODO handle m_bDispatcherFound
    m_bDispatcherFound = false;
    return false;
}

bool CGestures::onTouchUp(wlr_touch_up_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    // NOTE this is neccessary because onTouchDown might fail and exit without
    // updating gestures
    wf::touch::point_t lift_off_pos;
    try {
        lift_off_pos = m_sGestureState.fingers.at(ev->touch_id).current;
    } catch (const std::out_of_range&) {
        return false;
    }

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_UP,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {lift_off_pos.x, lift_off_pos.y},
    };

    IGestureManager::onTouchUp(gesture_event);

    // TODO where do I put this, before or after IGestureManager::onTouchUp...?
    if (m_bWorkspaceSwipeActive) {
        emulateSwipeEnd(ev->time_msec, false);
    }

    // TODO handle m_bDispatcherFound
    m_bDispatcherFound = false;
    return false;
}

bool CGestures::onTouchMove(wlr_touch_motion_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    auto pos = wlrTouchEventPositionAsPixels(ev->x, ev->y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = pos,
    };

    IGestureManager::onTouchMove(gesture_event);

    // TODO where do I put this
    if (m_bWorkspaceSwipeActive) {
        emulateSwipeUpdate(ev->time_msec);
    }

    // TODO handle m_bDispatcherFound
    m_bDispatcherFound = false;
    return false;
}

SMonitorArea CGestures::getMonitorArea() const {
    if (!m_pLastTouchedMonitor) {
        Debug::log(ERR, "[touch-gestures] m_pLastTouchedMonitor is null!");
        return {};
    }

    return m_sMonitorArea;
}

wf::touch::point_t CGestures::wlrTouchEventPositionAsPixels(double x,
                                                            double y) const {
    auto area = getMonitorArea();
    return wf::touch::point_t{x * area.w, y * area.h};
}
