#include "GestureManager.hpp"
#include "wayfire/touch/touch.hpp"
#include <algorithm>
#include <cstdint>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <memory>
#include <optional>

// constexpr double SWIPE_THRESHOLD = 30.;

bool handleGestureBind(std::string bind, bool pressed);

int handleTimer(void* data) {
    const auto gesture_manager = (GestureManager*)data;
    gesture_manager->onLongPressTimeout(gesture_manager->hold_gesture_next_trigger_time);

    return 0;
}

GestureManager::GestureManager() {
    static auto* const PSENSITIVITY =
        &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity")->floatValue;
    static auto* const HOLD_DELAY = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:hold_delay")->intValue;

    this->addMultiFingerGesture(PSENSITIVITY, HOLD_DELAY);
    this->addMultiFingerTap(PSENSITIVITY, HOLD_DELAY);
    this->addLongPress(PSENSITIVITY, HOLD_DELAY);
    this->addEdgeSwipeGesture(PSENSITIVITY, HOLD_DELAY);

    this->hold_gesture_timer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleTimer, this);
}

void GestureManager::emulateSwipeBegin(uint32_t time) {
    static auto* const PSWIPEFINGERS =
        &HyprlandAPI::getConfigValue(PHANDLE, "gestures:workspace_swipe_fingers")->intValue;

    // HACK .pointer is not used by g_pInputManager->onSwipeBegin so it's fine I
    // think
    auto emulated_swipe =
        wlr_pointer_swipe_begin_event{.pointer = nullptr, .time_msec = time, .fingers = (uint32_t)*PSWIPEFINGERS};
    g_pInputManager->onSwipeBegin(&emulated_swipe);

    m_vGestureLastCenter = m_sGestureState.get_center().origin;
}

void GestureManager::emulateSwipeEnd(uint32_t time, bool cancelled) {
    auto emulated_swipe = wlr_pointer_swipe_end_event{.pointer = nullptr, .time_msec = time, .cancelled = cancelled};
    g_pInputManager->onSwipeEnd(&emulated_swipe);
}

void GestureManager::emulateSwipeUpdate(uint32_t time) {
    static auto* const PSWIPEDIST =
        &HyprlandAPI::getConfigValue(PHANDLE, "gestures:workspace_swipe_distance")->intValue;

    if (!g_pInputManager->m_sActiveSwipe.pMonitor) {
        Debug::log(ERR, "ignoring touch gesture motion event due to missing monitor!");
        return;
    }

    auto currentCenter = m_sGestureState.get_center().current;

    // touch coords are within 0 to 1, we need to scale it with PSWIPEDIST for
    // one to one gesture
    auto emulated_swipe = wlr_pointer_swipe_update_event{
        .pointer   = nullptr,
        .time_msec = time,
        .fingers   = (uint32_t)m_sGestureState.fingers.size(),
        .dx        = (currentCenter.x - m_vGestureLastCenter.x) / m_sMonitorArea.w * *PSWIPEDIST,
        .dy        = (currentCenter.y - m_vGestureLastCenter.y) / m_sMonitorArea.h * *PSWIPEDIST};

    g_pInputManager->onSwipeUpdate(&emulated_swipe);
    m_vGestureLastCenter = currentCenter;
}

bool GestureManager::handleCompletedGesture(const CompletedGesture& gev) {
    return handleGestureBind(gev.to_string(), false);
}

bool GestureManager::handleDragGesture(const DragGesture& gev) {
    switch (gev.type) {
        case DragGestureType::SWIPE:
            return this->handleWorkspaceSwipe(gev);
        default:
            break;
    }

    return handleGestureBind(gev.to_string(), true);
}

// bind is the name of the gesture event.
// pressed only matters for mouse binds: only start of drag gestures should set it to true
bool handleGestureBind(std::string bind, bool pressed) {
    bool found = false;
    Debug::log(LOG, "[hyprgrass] Gesture Triggered: {}", bind);

    for (const auto& k : g_pKeybindManager->m_lKeybinds) {
        if (k.key != bind)
            continue;

        const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(k.mouse ? "mouse" : k.handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end()) {
            Debug::log(ERR, "Invalid handler in a keybind! (handler {} does not exist)", k.handler);
            continue;
        }

        // call the dispatcher
        Debug::log(LOG, "[hyprgrass] calling dispatcher ({})", bind);

        if (k.handler == "pass")
            continue;

        if (k.handler == "mouse") {
            DISPATCHER->second(pressed ? "1" : "0" + k.arg);
        } else {
            DISPATCHER->second(k.arg);
        }

        if (!k.nonConsuming) {
            found = true;
        }
    }

    return found;
}

void GestureManager::handleCancelledGesture() {
    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    // FIXME: make it so handleDragGestureEnd is called instead of handling this here
    switch (this->getActiveDragGesture()->type) {
        case DragGestureType::SWIPE:
            this->emulateSwipeEnd(0, false);
            return;
        case DragGestureType::HOLD:
            break;
    }
}

void GestureManager::dragGestureUpdate(const wf::touch::gesture_event_t& ev) {
    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    switch (this->getActiveDragGesture()->type) {
        case DragGestureType::SWIPE:
            emulateSwipeUpdate(ev.time);
            return;
        case DragGestureType::HOLD:
            // TODO:
            // g_pKeybindManager->mouseMoveUnified();
            return;
    }
}

void GestureManager::handleDragGestureEnd(const DragGesture& gev) {
    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    switch (this->getActiveDragGesture()->type) {
        case DragGestureType::SWIPE:
            emulateSwipeEnd(0, false);
            return;
        case DragGestureType::HOLD:
            break;
    }

    handleGestureBind(gev.to_string(), false);
}

bool GestureManager::handleWorkspaceSwipe(const DragGesture& gev) {
    static auto* const PWORKSPACEFINGERS =
        &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers")->intValue;
    const auto VERTANIMS = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace)
                               ->m_vRenderOffset.getConfig()
                               ->pValues->internalStyle == "slidevert";

    if (gev.type == DragGestureType::SWIPE && gev.finger_count == *PWORKSPACEFINGERS) {
        const auto horizontal           = GESTURE_DIRECTION_LEFT | GESTURE_DIRECTION_RIGHT;
        const auto vertical             = GESTURE_DIRECTION_UP | GESTURE_DIRECTION_DOWN;
        const auto workspace_directions = VERTANIMS ? vertical : horizontal;
        const auto anti_directions      = VERTANIMS ? horizontal : vertical;

        if (gev.direction & workspace_directions && !(gev.direction & anti_directions)) {
            // FIXME time arg of @emulateSwipeBegin should probably be assigned
            // something useful (though its not really used later)
            this->emulateSwipeBegin(0);
            return true;
        }
    }

    return false;
}

void GestureManager::updateLongPressTimer(uint32_t current_time, uint32_t delay) {
    this->hold_gesture_next_trigger_time = current_time + delay + 1;
    wl_event_source_timer_update(this->hold_gesture_timer, delay);
}

void GestureManager::stopLongPressTimer() {
    wl_event_source_timer_update(this->hold_gesture_timer, 0);
}

void GestureManager::sendCancelEventsToWindows() {
    static auto* const SEND_CANCEL =
        &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")->intValue;

    if (*SEND_CANCEL == 0) {
        return;
    }

    for (const auto& surface : this->touchedSurfaces) {
        if (!surface)
            continue;
        wlr_seat_touch_notify_cancel(g_pCompositor->m_sSeat.seat, surface);
    }
    this->touchedSurfaces.clear();
}

// @return whether or not to inhibit further actions
bool GestureManager::onTouchDown(wlr_touch_down_event* ev) {
    static auto* const SEND_CANCEL =
        &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")->intValue;

    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    if (!eventForwardingInhibited() && *SEND_CANCEL && g_pInputManager->m_sTouchData.touchFocusSurface) {
        // remember which surfaces were touched, to later send cancel events
        const auto surface = g_pInputManager->m_sTouchData.touchFocusSurface;
        const auto TOUCHED = std::find(touchedSurfaces.begin(), touchedSurfaces.end(), surface);
        if (TOUCHED == touchedSurfaces.end()) {
            touchedSurfaces.push_back(surface);
        }
    }

    m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(ev->touch->output_name ? ev->touch->output_name : "");

    const auto PDEVIT = std::find_if(g_pInputManager->m_lTouchDevices.begin(), g_pInputManager->m_lTouchDevices.end(),
                                     [&](const STouchDevice& other) { return other.pWlrDevice == &ev->touch->base; });

    if (PDEVIT != g_pInputManager->m_lTouchDevices.end() && !PDEVIT->boundOutput.empty()) {
        m_pLastTouchedMonitor = g_pCompositor->getMonitorFromName(PDEVIT->boundOutput);
    }
    m_pLastTouchedMonitor = m_pLastTouchedMonitor ? m_pLastTouchedMonitor : g_pCompositor->m_pLastMonitor;

    const auto& position = m_pLastTouchedMonitor->vecPosition;
    const auto& geometry = m_pLastTouchedMonitor->vecSize;
    this->m_sMonitorArea = {position.x, position.y, geometry.x, geometry.y};

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

    return IGestureManager::onTouchDown(gesture_event);
}

bool GestureManager::onTouchUp(wlr_touch_up_event* ev) {
    static auto* const SEND_CANCEL =
        &HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")->intValue;

    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    wf::touch::point_t lift_off_pos;
    try {
        lift_off_pos = this->m_sGestureState.fingers.at(ev->touch_id).current;
    } catch (const std::out_of_range&) {
        return false;
    }

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_UP,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = {lift_off_pos.x, lift_off_pos.y},
    };

    const auto BLOCK = IGestureManager::onTouchUp(gesture_event);
    if (*SEND_CANCEL) {
        return BLOCK;
    } else {
        // send_cancel is turned off; we need to rely on touchup events
        return false;
    }
}

bool GestureManager::onTouchMove(wlr_touch_motion_event* ev) {
    if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
        return false;

    auto pos = wlrTouchEventPositionAsPixels(ev->x, ev->y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev->time_msec,
        .finger = ev->touch_id,
        .pos    = pos,
    };

    return IGestureManager::onTouchMove(gesture_event);
}

SMonitorArea GestureManager::getMonitorArea() const {
    return this->m_sMonitorArea;
}

void GestureManager::onLongPressTimeout(uint32_t time_msec) {
    if (this->m_sGestureState.fingers.empty()) {
        return;
    }

    const auto finger = this->m_sGestureState.fingers.begin();

    const wf::touch::gesture_event_t touch_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = time_msec,
        .finger = finger->first,
        .pos    = finger->second.current,
    };

    IGestureManager::onTouchMove(touch_event);
}

wf::touch::point_t GestureManager::wlrTouchEventPositionAsPixels(double x, double y) const {
    auto area = this->getMonitorArea();
    // TODO do I need to add area.x and area.y respectively?
    return wf::touch::point_t{x * area.w, y * area.h};
}
