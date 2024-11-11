#include "GestureManager.hpp"
#include "globals.hpp"
#include "wayfire/touch/touch.hpp"
#include <algorithm>
#include <cstdint>
#include <hyprutils/memory/SharedPtr.hpp>
#include <string>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>
#undef private

#include <algorithm>
#include <hyprlang.hpp>
#include <memory>
#include <optional>
#include <ranges>

// constexpr double SWIPE_THRESHOLD = 30.;
constexpr int RESIZE_BORDER_GAP_INCREMENT = 10;

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::vector<std::string> splitString(const std::string& s, char delimiter, int numSubstrings) {
    std::vector<std::string> substrings;
    auto split_view = std::ranges::views::split(s, delimiter);
    auto iter       = split_view.begin();

    for (int i = 0; i < numSubstrings - 1 && iter != split_view.end(); ++i, ++iter) {
        std::string substring;
        for (char c : *iter) {
            substring.push_back(c);
        }
        substrings.push_back(substring);
    }

    if (iter != split_view.end()) {
        std::string rest;
        for (char c : *iter) {
            rest.push_back(c);
        }
        substrings.push_back(rest);
    }

    return substrings;
}

int handleLongPressTimer(void* data) {
    const auto gesture_manager = (GestureManager*)data;
    gesture_manager->onLongPressTimeout(gesture_manager->long_press_next_trigger_time);

    return 0;
}

std::string commaSeparatedCssGaps(CCssGapData data) {
    return std::to_string(data.top) + "," + std::to_string(data.right) + "," + std::to_string(data.bottom) + "," +
           std::to_string(data.left);
}

GestureManager::GestureManager() : IGestureManager(std::make_unique<HyprLogger>()) {
    static auto const PSENSITIVITY =
        (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity")
            ->getDataStaticPtr();
    static auto const LONG_PRESS_DELAY =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:long_press_delay")
            ->getDataStaticPtr();
    static auto const EDGE_MARGIN =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:edge_margin")
            ->getDataStaticPtr();

    this->addMultiFingerGesture(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addMultiFingerTap(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addLongPress(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addEdgeSwipeGesture(*PSENSITIVITY, *LONG_PRESS_DELAY, *EDGE_MARGIN);

    this->long_press_timer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, handleLongPressTimer, this);
}

GestureManager::~GestureManager() {
    wl_event_source_remove(this->long_press_timer);
}

bool GestureManager::handleCompletedGesture(const CompletedGestureEvent& gev) {
    return this->handleGestureBind(gev.to_string(), false);
}

bool GestureManager::handleDragGesture(const DragGestureEvent& gev) {
    static auto const WORKSPACE_SWIPE_FINGERS =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers")
            ->getDataStaticPtr();
    static auto const WORKSPACE_SWIPE_EDGE =
        (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_edge")
            ->getDataStaticPtr();
    static auto const RESIZE_LONG_PRESS =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:resize_on_border_long_press")
            ->getDataStaticPtr();

    static auto PBORDERSIZE =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->getDataStaticPtr();
    static auto PBORDERGRABEXTEND =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "general:extend_border_grab_area")
            ->getDataStaticPtr();
    static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");

    Debug::log(LOG, "[hyprgrass] Drag gesture begin: {}", gev.to_string());

    auto const workspace_swipe_edge_str = std::string{*WORKSPACE_SWIPE_EDGE};

    switch (gev.type) {
        case DragGestureType::SWIPE: {
            if (**WORKSPACE_SWIPE_FINGERS == gev.finger_count) {
                if (!g_pGestureManager->handleWorkspaceSwipe(gev.direction)) {
                    static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent("hyprgrass:swipeStart");
                    SCallbackInfo info;

                    g_pHookSystem->emit(PEVENTVEC, info,
                                        IPointer::SSwipeBeginEvent{.fingers = (uint32_t)gev.finger_count});

                    if (!info.cancelled) {
                        this->hookHandled        = true;
                        this->previousSwipePoint = this->m_sGestureState.get_center().delta();
                        return true;
                    }
                } else {
                    this->hookHandled = false;
                    return true;
                }
            }
            return false;
        }

        case DragGestureType::EDGE_SWIPE:
            if (workspace_swipe_edge_str == "l" && gev.edge_origin == GESTURE_DIRECTION_LEFT) {
                return this->handleWorkspaceSwipe(gev.direction);
            }
            if (workspace_swipe_edge_str == "r" && gev.edge_origin == GESTURE_DIRECTION_RIGHT) {
                return this->handleWorkspaceSwipe(gev.direction);
            }
            if (workspace_swipe_edge_str == "u" && gev.edge_origin == GESTURE_DIRECTION_UP) {
                return this->handleWorkspaceSwipe(gev.direction);
            }
            if (workspace_swipe_edge_str == "d" && gev.edge_origin == GESTURE_DIRECTION_DOWN) {
                return this->handleWorkspaceSwipe(gev.direction);
            }

            return false;

        case DragGestureType::LONG_PRESS:
            if (**RESIZE_LONG_PRESS && gev.finger_count == 1) {
                const auto BORDER_GRAB_AREA = **PBORDERSIZE + **PBORDERGRABEXTEND;

                // kind of a hack: this is the window detected from previous touch events
                const auto w = g_pInputManager->m_pFoundWindowToFocus.lock();
                const Vector2D touchPos =
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().current) *
                    this->m_pLastTouchedMonitor->vecSize;
                if (w && !w->isFullscreen()) {
                    const CBox real = {w->m_vRealPosition.value().x, w->m_vRealPosition.value().y,
                                       w->m_vRealSize.value().x, w->m_vRealSize.value().y};
                    const CBox grab = {real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA,
                                       real.width + 2 * BORDER_GRAB_AREA, real.height + 2 * BORDER_GRAB_AREA};

                    bool notInRealWindow = !real.containsPoint(touchPos) || w->isInCurvedCorner(touchPos.x, touchPos.y);
                    bool onTiledGap      = !w->m_bIsFloating && !w->isFullscreen() && notInRealWindow;
                    bool inGrabArea      = notInRealWindow && grab.containsPoint(touchPos);

                    if ((onTiledGap || inGrabArea) && !w->hasPopupAt(touchPos)) {
                        IPointer::SButtonEvent e = {
                            .timeMs = 0, // HACK: they don't use this :p
                            .button = 0,
                            .state  = WL_POINTER_BUTTON_STATE_PRESSED,
                        };
                        g_pKeybindManager->resizeWithBorder(e);

                        auto* PGAPSIN            = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
                        this->resizeOnBorderInfo = {
                            .active      = true,
                            .old_gaps_in = *PGAPSIN,
                        };

                        CCssGapData newGapsIn = *PGAPSIN;
                        newGapsIn.top += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.right += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.bottom += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.left += RESIZE_BORDER_GAP_INCREMENT;
                        g_pConfigManager->parseKeyword("general:gaps_in", commaSeparatedCssGaps(newGapsIn));
                        return true;
                    }
                }
            }
            return this->handleGestureBind(gev.to_string(), true);
    }

    return false;
}

// bind is the name of the gesture event.
// pressed only matters for mouse binds: only start of drag gestures should set it to true
bool GestureManager::handleGestureBind(std::string bind, bool pressed) {
    bool found = false;
    Debug::log(LOG, "[hyprgrass] Looking for binds matching: {}", bind);

    auto allBinds = std::ranges::views::join(std::array{g_pKeybindManager->m_vKeybinds, this->internalBinds});

    for (const auto& k : allBinds) {
        if (k->key != bind)
            continue;

        const auto DISPATCHER = g_pKeybindManager->m_mDispatchers.find(k->mouse ? "mouse" : k->handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == g_pKeybindManager->m_mDispatchers.end()) {
            Debug::log(ERR, "Invalid handler in a keybind! (handler {} does not exist)", k->handler);
            continue;
        }

        // call the dispatcher
        Debug::log(LOG, "[hyprgrass] calling dispatcher ({})", bind);

        if (k->handler == "pass")
            continue;

        if (k->handler == "mouse") {
            DISPATCHER->second((pressed ? "1" : "0") + k->arg);
        } else {
            DISPATCHER->second(k->arg);
        }

        if (!k->nonConsuming) {
            found = true;
        }
    }

    return found;
}

void GestureManager::handleCancelledGesture() {}

void GestureManager::dragGestureUpdate(const wf::touch::gesture_event_t& ev) {

    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    switch (this->getActiveDragGesture()->type) {
        case DragGestureType::SWIPE: {
            if (this->hookHandled) {
                this->updateHookSwipe();
            } else {
                this->updateWorkspaceSwipe();
            }
            return;
        }
        case DragGestureType::LONG_PRESS: {
            const auto pos = this->m_sGestureState.get_center().current;
            g_pCompositor->warpCursorTo(Vector2D(pos.x, pos.y));
            g_pInputManager->mouseMoveUnified(ev.time);
            return;
        }
        case DragGestureType::EDGE_SWIPE:
            this->updateWorkspaceSwipe();
    }
}

void GestureManager::handleDragGestureEnd(const DragGestureEvent& gev) {
    Debug::log(LOG, "[hyprgrass] Drag gesture ended: {}", gev.to_string());

    switch (gev.type) {
        case DragGestureType::SWIPE: {
            if (this->hookHandled) {
                IPointer::SSwipeEndEvent ev = {};
                EMIT_HOOK_EVENT("hyprgrass:swipeEnd", ev);
            } else {
                g_pInputManager->endWorkspaceSwipe();
            }
            return;
        }
        case DragGestureType::LONG_PRESS:
            if (this->resizeOnBorderInfo.active) {
                g_pKeybindManager->changeMouseBindMode(eMouseBindMode::MBIND_INVALID);
                g_pConfigManager->parseKeyword("general:gaps_in",
                                               commaSeparatedCssGaps(this->resizeOnBorderInfo.old_gaps_in));
                this->resizeOnBorderInfo = {};
                return;
            }

            this->handleGestureBind(gev.to_string(), false);
            return;
        case DragGestureType::EDGE_SWIPE:
            g_pInputManager->endWorkspaceSwipe();
            return;
    }
}

bool GestureManager::handleWorkspaceSwipe(const GestureDirection direction) {
    const bool VERTANIMS =
        g_pCompositor->m_pLastMonitor->activeWorkspace->m_vRenderOffset.getConfig()->pValues->internalStyle ==
            "slidevert" ||
        g_pCompositor->m_pLastMonitor->activeWorkspace->m_vRenderOffset.getConfig()->pValues->internalStyle.starts_with(
            "slidevert");

    const auto horizontal           = GESTURE_DIRECTION_LEFT | GESTURE_DIRECTION_RIGHT;
    const auto vertical             = GESTURE_DIRECTION_UP | GESTURE_DIRECTION_DOWN;
    const auto workspace_directions = VERTANIMS ? vertical : horizontal;
    const auto anti_directions      = VERTANIMS ? horizontal : vertical;

    if (direction & workspace_directions && !(direction & anti_directions)) {
        g_pInputManager->beginWorkspaceSwipe();
        return true;
    }

    return false;
}

void GestureManager::updateWorkspaceSwipe() {
    const bool VERTANIMS =
        g_pInputManager->m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()->pValues->internalStyle ==
            "slidevert" ||
        g_pInputManager->m_sActiveSwipe.pWorkspaceBegin->m_vRenderOffset.getConfig()
            ->pValues->internalStyle.starts_with("slidefadevert");

    static auto const PSWIPEDIST =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "gestures:workspace_swipe_distance")
            ->getDataStaticPtr();
    const auto SWIPEDISTANCE = std::clamp(**PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);

    const auto monArea       = this->getMonitorArea();
    const auto delta_percent = this->m_sGestureState.get_center().delta() / wf::touch::point_t(monArea.w, monArea.h);

    const auto swipe_delta = Vector2D(delta_percent.x * SWIPEDISTANCE, delta_percent.y * SWIPEDISTANCE);

    g_pInputManager->updateWorkspaceSwipe(VERTANIMS ? -swipe_delta.y : -swipe_delta.x);
    return;
}

void GestureManager::updateHookSwipe() {
    auto gev = this->getActiveDragGesture().value();

    const auto currentPoint  = this->m_sGestureState.get_center().delta();
    const auto delta         = currentPoint - this->previousSwipePoint;
    this->previousSwipePoint = currentPoint;

    IPointer::SSwipeUpdateEvent ev = {
        .fingers = (uint32_t)gev.finger_count,
        .delta   = Vector2D(delta.x, delta.y),
    };
    EMIT_HOOK_EVENT("hyprgrass:swipeUpdate", ev);
}

void GestureManager::updateLongPressTimer(uint32_t current_time, uint32_t delay) {
    this->long_press_next_trigger_time = current_time + delay + 1;
    wl_event_source_timer_update(this->long_press_timer, delay);
}

void GestureManager::stopLongPressTimer() {
    wl_event_source_timer_update(this->long_press_timer, 0);
}

void GestureManager::sendCancelEventsToWindows() {
    static auto const SEND_CANCEL =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")
            ->getDataStaticPtr();

    if (!**SEND_CANCEL) {
        return;
    }

    for (const auto& touch : this->touchedResources.all()) {
        const auto t = touch.lock();
        if (t.get()) {
            t->sendCancel();
        }
    }
}

// @return whether or not to inhibit further actions
bool GestureManager::onTouchDown(ITouch::SDownEvent ev) {
    static auto const SEND_CANCEL =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")
            ->getDataStaticPtr();

    // if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
    //     return false;

    this->m_pLastTouchedMonitor =
        g_pCompositor->getMonitorFromName(!ev.device->boundOutput.empty() ? ev.device->boundOutput : "");

    this->m_pLastTouchedMonitor =
        this->m_pLastTouchedMonitor ? this->m_pLastTouchedMonitor : g_pCompositor->m_pLastMonitor.lock();

    const auto& monitorPos  = m_pLastTouchedMonitor->vecPosition;
    const auto& monitorSize = m_pLastTouchedMonitor->vecSize;
    this->m_sMonitorArea    = {monitorPos.x, monitorPos.y, monitorSize.x, monitorSize.y};

    g_pCompositor->warpCursorTo({
        monitorPos.x + ev.pos.x * monitorSize.x,
        monitorPos.y + ev.pos.y * monitorSize.y,
    });

    g_pInputManager->refocus();

    if (!eventForwardingInhibited() && **SEND_CANCEL && g_pInputManager->m_sTouchData.touchFocusSurface) {
        // remember which surfaces were touched, to later send cancel events
        const auto surface = g_pInputManager->m_sTouchData.touchFocusSurface;

        if (this->m_sGestureState.fingers.size() == 0) {
            this->touchedResources.clear();
        }

        wl_client* client = surface.get()->client();
        if (client) {
            SP<CWLSeatResource> seat = g_pSeatManager->seatResourceForClient(client);

            if (seat) {
                auto touches = seat.get()->touches;
                for (const auto& touch : touches) {
                    this->touchedResources.insert(touch);
                }
            }
        }
    }

    // NOTE @wlr_touch_down_event.x and y uses a number between 0 and 1 to
    // represent "how many percent of screen" whereas
    // @wf::touch::gesture_event_t uses PIXELS as unit
    auto pos = wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_DOWN,
        .time   = ev.timeMs,
        .finger = ev.touchID,
        .pos    = pos,
    };

    return IGestureManager::onTouchDown(gesture_event);
}

bool GestureManager::onTouchUp(ITouch::SUpEvent ev) {
    static auto const SEND_CANCEL =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")
            ->getDataStaticPtr();

    // if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
    //     return false;

    wf::touch::point_t lift_off_pos;
    try {
        lift_off_pos = this->m_sGestureState.fingers.at(ev.touchID).current;
    } catch (const std::out_of_range&) {
        return false;
    }

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_TOUCH_UP,
        .time   = ev.timeMs,
        .finger = ev.touchID,
        .pos    = {lift_off_pos.x, lift_off_pos.y},
    };

    const auto BLOCK = IGestureManager::onTouchUp(gesture_event);
    if (**SEND_CANCEL) {
        const auto surface = g_pInputManager->m_sTouchData.touchFocusSurface;

        if (!surface.valid()) {
            return true;
        }

        wl_client* client = surface.get()->client();
        if (!client) {
            return true;
        }

        SP<CWLSeatResource> seat = g_pSeatManager->seatResourceForClient(client);
        if (!seat.get()) {
            return true;
        }

        auto touches = seat.get()->touches;
        for (const auto& touch : touches) {
            this->touchedResources.remove(touch);
        }

        return BLOCK;
    } else {
        // send_cancel is turned off; we need to rely on touchup events
        return false;
    }
}

bool GestureManager::onTouchMove(ITouch::SMotionEvent ev) {
    // if (g_pCompositor->m_sSeat.exclusiveClient) // lock screen, I think
    //     return false;

    auto pos = wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev.timeMs,
        .finger = ev.touchID,
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
    return wf::touch::point_t{x * area.w + area.x, y * area.h + area.y};
}

Vector2D GestureManager::pixelPositionToPercentagePosition(wf::touch::point_t point) const {
    auto monitorArea = this->getMonitorArea();
    return Vector2D((point.x - monitorArea.x) / monitorArea.w, (point.y - monitorArea.y) / monitorArea.h);
}

void GestureManager::touchBindDispatcher(std::string args) {
    auto argsSplit = splitString(args, ',', 4);
    if (argsSplit.size() < 4) {
        Debug::log(ERR, "touchBind called with not enough args: {}", args);
        return;
    }
    const auto _modifier      = trim(argsSplit[0]);
    const auto key            = trim(argsSplit[1]);
    const auto dispatcher     = trim(argsSplit[2]);
    const auto dispatcherArgs = trim(argsSplit[3]);

    this->internalBinds.emplace_back(makeShared<SKeybind>(SKeybind{
        .key     = key,
        .handler = dispatcher,
        .arg     = dispatcherArgs,
    }));
}
