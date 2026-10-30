#include "GestureManager.hpp"
#include "HyprLogger.hpp"
#include "config/lua/LuaRef.hpp"
#include "gestures/Gestures.hpp"

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/lua/ConfigManager.hpp>
#include <hyprland/src/config/lua/LuaCaller.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/config/supplementary/propRefresher/PropRefresher.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/keybinds/Manager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/UnifiedWorkspaceSwipeGesture.hpp>
#include <hyprland/protocols/wlr-layer-shell-unstable-v1.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#undef private

#include <hyprutils/string/Numeric.hpp>

#include <algorithm>
#include <cmath>
#include <linux/input-event-codes.h>
#include <ranges>
#include <vector>

constexpr int RESIZE_BORDER_GAP_INCREMENT = 10;

static int handleLongPressTimer(void* data) {
    const auto gesture_manager = (GestureManager*)data;
    gesture_manager->onLongPressTimeout(gesture_manager->long_press_next_trigger_time);

    return 0;
}

// The fingers of a gesture land within about 25 ms of each other; a touch that moves this far (logical px)
// first is a scroll or a pinch, not a gesture waiting for more fingers.
constexpr static uint32_t HOLD_MS   = 40;
constexpr static double   HOLD_SLOP = 12;

static int handleHoldTimer(void* data) {
    ((GestureManager*)data)->onHoldTimeout();
    return 0;
}

static void updateGapsIn(const Config::CCssGapData& newGapsIn) {
    static auto PGAPSINDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");

    auto luaMgr = dynamicPointerCast<Config::Lua::CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));

    const auto it = luaMgr->m_configValues.find("general.gaps_in");
    if (it == luaMgr->m_configValues.end()) {
        LOG(Log::ERR, "[hyprgrass] lua config 'general.gaps_in' not found");
        return;
    }

    auto* gapsInPtr = dynamic_cast<Config::CCssGapData*>(PGAPSINDATA.ptr());
    // idk why `*gapsInPtr = newGapsIn` doesn't work
    gapsInPtr->m_bottom = newGapsIn.m_bottom;
    gapsInPtr->m_top    = newGapsIn.m_top;
    gapsInPtr->m_left   = newGapsIn.m_left;
    gapsInPtr->m_right  = newGapsIn.m_right;
    Config::Supplementary::refresher()->scheduleRefresh(it->second->refreshBits());
}

GestureManager::GestureManager() : IGestureManager(std::make_unique<HyprLogger>()) {
    static auto const PSENSITIVITY     = g_config->sensitivity;
    static auto const LONG_PRESS_DELAY = g_config->longPressDelay;
    static auto const EDGE_MARGIN      = g_config->edgeMargin;
    const auto sensitivity             = PSENSITIVITY->m_val.ptr();
    const auto longPressDelay          = LONG_PRESS_DELAY->m_val.ptr();
    const auto margin                  = EDGE_MARGIN->m_val.ptr();

    this->addEdgeSwipeGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, longPressDelay, margin);
    // TODO: should I use SWIPE_INCORRECT_DRAG_TOLERANCE instead?
    this->addLongPress(SWIPE_THRESHOLD, sensitivity, longPressDelay);
    this->addMultiFingerGesture(SWIPE_THRESHOLD, SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, longPressDelay);
    this->addMultiFingerTap(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, longPressDelay);
    this->addPinchGesture(PINCH_THRESHOLD, sensitivity, longPressDelay);

    this->long_press_timer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleLongPressTimer, this);
    this->holdTimer        = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleHoldTimer, this);
}

GestureManager::~GestureManager() {
    wl_event_source_remove(this->long_press_timer);
    wl_event_source_remove(this->holdTimer);
}

FindGestureResult GestureManager::findCompletedGesture(const CompletedGestureEvent& gev) const {
    return this->findGestureBind(gev.to_string(), GestureEventType::COMPLETED);
}
FindGestureResult GestureManager::handleCompletedGesture(const CompletedGestureEvent& gev) {
    return this->handleGestureBind(gev.to_string(), GestureEventType::COMPLETED);
}

bool GestureManager::handleDragGesture(const DragGestureEvent& gev) {
    static auto const WORKSPACE_SWIPE_FINGERS = g_config->workspaceSwipeFingers;
    static auto const WORKSPACE_SWIPE_EDGE    = g_config->workspaceSwipeEdge;
    static auto const RESIZE_LONG_PRESS       = g_config->resizeOnBorder;

    static auto PBORDERSIZE       = CConfigValue<Config::INTEGER>("general:border_size");
    static auto PBORDERGRABEXTEND = CConfigValue<Config::INTEGER>("general:extend_border_grab_area");
    static auto PGAPSINDATA       = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");

    LOG(Log::DEBUG, "[hyprgrass] Drag gesture begin: {}", gev.to_string());

    auto const workspace_swipe_edge_str = WORKSPACE_SWIPE_EDGE->value();

    switch (gev.type) {
        case GestureType::SWIPE: {
            if (this->trackpadGestureBegin(gev))
                return true;

            if (WORKSPACE_SWIPE_FINGERS->value() != gev.finger_count) {
                return false;
            }

            if (this->handleWorkspaceSwipe(gev.direction))
                return true;

            return false;
        }

        case GestureType::EDGE_SWIPE: {
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

            if (this->trackpadGestureBegin(gev))
                return true;

            return false;
        }

        case GestureType::LONG_PRESS:
            if (g_pSessionLockManager->isSessionLocked()) {
                return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN) !=
                       FindGestureResult::NONE;
            }

            if (RESIZE_LONG_PRESS->value() && gev.finger_count == 1) {
                const auto BORDER_GRAB_AREA = *PBORDERSIZE + *PBORDERGRABEXTEND;

                // kind of a hack: this is the window detected from previous touch events
                const auto w = g_pInputManager->m_foundWindowToFocus.lock();
                const Vector2D touchPos =
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().current) *
                    this->m_lastTouchedMonitor->m_size;
                if (w && !Fullscreen::controller()->isFullscreen(w)) {
                    const Vector2D realPos  = w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
                    const Vector2D realSize = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
                    const CBox real         = {realPos.x, realPos.y, realSize.x, realSize.y};
                    const CBox grab         = {
                        real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA, real.width + 2 * BORDER_GRAB_AREA,
                        real.height + 2 * BORDER_GRAB_AREA
                    };

                    bool notInRealWindow =
                        !real.containsPoint(touchPos) || w->presentation().isInCurvedCorner(touchPos.x, touchPos.y);
                    bool onTiledGap = !w->isFloating() && !Fullscreen::controller()->isFullscreen(w) && notInRealWindow;
                    bool inGrabArea = notInRealWindow && grab.containsPoint(touchPos);

                    if ((onTiledGap || inGrabArea) && !w->hasPopupAt(touchPos)) {
                        if (!g_layoutManager->dragController()->target())
                            g_layoutManager->beginDragTarget(w->layoutTarget(), MBIND_RESIZE);

                        auto oldGapsIn           = *static_cast<Config::CCssGapData*>(PGAPSINDATA.ptr());
                        this->resizeOnBorderInfo = {
                            .active      = true,
                            .old_gaps_in = oldGapsIn,
                        };

                        Config::CCssGapData newGapsIn = oldGapsIn;
                        newGapsIn.m_top += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_right += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_bottom += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_left += RESIZE_BORDER_GAP_INCREMENT;
                        updateGapsIn(newGapsIn);
                        return true;
                    }
                }
            }

            if (this->trackpadGestureBegin(gev))
                return true;

            return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN) != FindGestureResult::NONE;

        case GestureType::PINCH:
            if (this->trackpadGestureBegin(gev))
                return true;

            return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN) != FindGestureResult::NONE;
            break;
        case GestureType::TAP:
            // tap does not trigger drag
            break;
    }

    return false;
}

// Adapt new hyprland keybind type back to the old one (hacky compat for now)
static std::vector<SP<SKeybind>> keybindsToSKeybinds() {
    std::vector<SP<SKeybind>> binds;
    for (const auto& k : Keybinds::mgr()->registry().binds())
        binds.emplace_back(
            makeShared<SKeybind>(SKeybind{
                .key          = k->metadata().displayKey,
                .handler      = k->metadata().handler,
                .arg          = k->metadata().argument,
                .displayKey   = k->metadata().displayKey,
                .mouse        = k->hasFlag(Keybinds::BIND_FLAG_MOUSE),
                .locked       = k->hasFlag(Keybinds::BIND_FLAG_LOCKED),
                .nonConsuming = k->hasFlag(Keybinds::BIND_FLAG_NON_CONSUMING),
                .modmask      = static_cast<uint32_t>(k->modifierMask()),
            })
        );
    return binds;
}

FindGestureResult GestureManager::findGestureBind(std::string bind, GestureEventType type) const {
    LOG(Log::DEBUG, "[hyprgrass] Looking for binds matching: {}", bind);

    auto result = FindGestureResult::NONE;

    auto allBinds   = std::ranges::views::join(std::array{keybindsToSKeybinds(), this->internalBinds});
    const auto MODS = static_cast<uint32_t>(g_pInputManager->getModsFromAllKBs());

    for (const auto& k : allBinds) {
        if (k->key != bind)
            continue;

        if (k->handler == "pass")
            continue;

        if (k->locked != g_pSessionLockManager->isSessionLocked())
            continue;

        if (k->modmask != MODS)
            continue;

        if (k->nonConsuming) {
            result = FindGestureResult::NON_CONSUMING;
            continue;
        }

        return FindGestureResult::FOUND;
    }
    return result;
}

// bind is the name of the gesture event.
// pressed only matters for mouse binds: only start of drag gestures should set it to true
FindGestureResult GestureManager::handleGestureBind(std::string bind, GestureEventType type) {
    auto found = FindGestureResult::NONE;
    LOG(Log::DEBUG, "[hyprgrass] Looking for binds matching: {}", bind);

    auto allBinds   = std::ranges::views::join(std::array{keybindsToSKeybinds(), this->internalBinds});
    const auto MODS = static_cast<uint32_t>(g_pInputManager->getModsFromAllKBs());

    for (const auto& k : allBinds) {
        if (k->key != bind)
            continue;

        if (k->handler == "pass")
            continue;

        if (k->locked != g_pSessionLockManager->isSessionLocked())
            continue;

        if (k->modmask != MODS)
            continue;

        // Should never happen, as we check in the ConfigManager, but oh well
        if (k->handler != "__lua" || !k->luaFn.state()) {
            LOG(Log::ERR, "Invalid handler in a keybind! (handler {} is not a lua function)", k->handler);
            continue;
        }

        auto luaMgr = dynamicPointerCast<Config::Lua::CConfigManager>(WP<Config::IConfigManager>(Config::mgr()));

        switch (type) {
            case GestureEventType::COMPLETED:
                // mouse dispatchers only trigger on drag begin/end
                if (!k->mouse) {
                    LOG(Log::DEBUG, "[hyprgrass] calling dispatcher ({})", bind);
                    (void)luaMgr->caller().call(k->luaFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "hyprgrass bind callback");
                    found =
                        std::max(found, k->nonConsuming ? FindGestureResult::NON_CONSUMING : FindGestureResult::FOUND);
                }
                break;

            default: {
                if (!k->mouse) {
                    // only mouse actions are considered on drag begin/end
                    continue;
                }

                bool pressed          = type == GestureEventType::DRAG_BEGIN;
                this->mouseBindActive = pressed;
                // yes this is how the lua dispatcher detects key press state
                Config::Actions::state()->m_passPressed = static_cast<int>(pressed);

                (void)luaMgr->caller().call(k->luaFn, Config::Lua::CLuaCaller::LUA_TIMEOUT_KEYBIND_CALLBACK_MS, "hyprgrass bind callback");

                Config::Actions::state()->m_passPressed = -1;

                found = std::max(found, k->nonConsuming ? FindGestureResult::NON_CONSUMING : FindGestureResult::FOUND);
            }
        }
    }

    return found;
}

void GestureManager::handleCancelledGesture() {}

void GestureManager::dragGestureUpdate(const wf::touch::gesture_event_t& ev) {
    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    if (this->activeTrackpadGesture) {
        this->trackpadGestureUpdate(ev.time);
        return;
    }

    switch (this->getActiveDragGesture()->type) {
        case GestureType::SWIPE:
            if (this->workspaceSwipeActive) {
                this->updateWorkspaceSwipe();
            }
            return;

        case GestureType::LONG_PRESS: {
            const auto pos = this->m_sGestureState.get_center().current;
            Pointer::pointerController()->warpTo(Vector2D(pos.x, pos.y));
            g_pInputManager->simulateMouseMovement();
            return;
        }
        case GestureType::EDGE_SWIPE:
            this->updateWorkspaceSwipe();
            break;

        case GestureType::PINCH:
            break;
        case GestureType::TAP:
            // tap does not trigger drag
            break;
    }
}

void GestureManager::handleDragGestureEnd(const DragGestureEvent& gev) {
    if (g_pSessionLockManager->isSessionLocked()) {
        this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
        return;
    }

    if (this->activeTrackpadGesture) {
        this->trackpadGestureEnd(gev.time);
        return;
    }

    LOG(Log::DEBUG, "[hyprgrass] Drag gesture ended: {}", gev.to_string());
    switch (gev.type) {
        case GestureType::SWIPE:
            if (this->workspaceSwipeActive) {
                g_pUnifiedWorkspaceSwipe->end();
                this->workspaceSwipeActive = false;
            }
            return;
        case GestureType::LONG_PRESS:
            if (this->resizeOnBorderInfo.active) {
                g_layoutManager->dragController()->dragEnd();
                updateGapsIn(this->resizeOnBorderInfo.old_gaps_in);
                this->resizeOnBorderInfo = {};
                return;
            }

            // longpress already triggered CompletedGesture on timeout
            if (this->mouseBindActive) {
                this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
            }

            return;
        case GestureType::EDGE_SWIPE:
            if (this->workspaceSwipeActive) {
                g_pUnifiedWorkspaceSwipe->end();
            }
            break;
        case GestureType::PINCH:
            this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
            return;
        case GestureType::TAP:
            // tap does not trigger drag
            break;
    }
}

bool GestureManager::handleWorkspaceSwipe(const GestureDirection direction) {
    const bool VERTANIMS =
        Desktop::focusState()->monitor()->m_activeWorkspace->m_renderOffset->getConfig()->pValues->internalStyle ==
            "slidevert" ||
        Desktop::focusState()
            ->monitor()
            ->m_activeWorkspace->m_renderOffset->getConfig()
            ->pValues->internalStyle.starts_with("slidevert");

    const auto horizontal           = GESTURE_DIRECTION_LEFT | GESTURE_DIRECTION_RIGHT;
    const auto vertical             = GESTURE_DIRECTION_UP | GESTURE_DIRECTION_DOWN;
    const auto workspace_directions = VERTANIMS ? vertical : horizontal;
    const auto anti_directions      = VERTANIMS ? horizontal : vertical;

    if (direction & workspace_directions && !(direction & anti_directions)) {
        this->workspaceSwipeActive = true;
        g_pUnifiedWorkspaceSwipe->begin();
        return true;
    }

    return false;
}

void GestureManager::updateWorkspaceSwipe() {
    const auto ANIMSTYLE   = g_pUnifiedWorkspaceSwipe->m_workspaceBegin->m_renderOffset->getStyle();
    const bool VERTANIMS   = ANIMSTYLE == "slidevert" || ANIMSTYLE.starts_with("slidefadevert");
    const auto swipe_delta = this->pixelToTrackpadDistance(this->m_sGestureState.get_center().delta());

    g_pUnifiedWorkspaceSwipe->update(VERTANIMS ? -swipe_delta.y : -swipe_delta.x);
    return;
}

bool GestureManager::trackpadGestureBegin(const DragGestureEvent& gev) {
    Vector2D delta = this->pixelToTrackpadDistance(this->m_sGestureState.get_center().delta());

    // longpress events do not trigger a handler->m_activeGesture at the beginning,
    // we look it up ourselves beforehand
    bool foundLongPress = false;
    // hyprland has an arbitrary threshold of 5 pixels
    if (gev.type == GestureType::LONG_PRESS && std::abs(delta.x) < 5 && std::abs(delta.y) < 5) {
        const auto MODS = g_pInputManager->getModsFromAllKBs();
        for (const auto& g : g_pShimTrackpadGestures->longPress()->m_gestures) {
            if (g->fingerCount == gev.finger_count && g->modMask == MODS) {
                foundLongPress = true;
                break;
            }
        }
    }
    uint32_t fingers = gev.type == GestureType::EDGE_SWIPE
                           ? (gev.edge_origin << MOD_MASK_SHIFT) | (gev.finger_count & FINGERS_MASK)
                           : gev.finger_count;

    CTrackpadGestures* handler = g_pShimTrackpadGestures->get(gev.type);
    if (gev.type == GestureType::PINCH) {
        IPointer::SPinchBeginEvent pinchBegin = {
            .timeMs  = gev.time,
            .fingers = fingers,
        };
        IPointer::SPinchUpdateEvent pinch = {
            .timeMs   = gev.time,
            .fingers  = fingers,
            .delta    = delta,
            .scale    = this->m_sGestureState.get_pinch_scale(),
            .rotation = this->m_sGestureState.get_rotation_angle(),
        };

        handler->gestureBegin(pinchBegin);
        handler->gestureUpdate(pinch);
    } else {
        IPointer::SSwipeBeginEvent swipeBegin = {
            .timeMs  = gev.time,
            .fingers = fingers,
        };
        IPointer::SSwipeUpdateEvent swipe = {
            .timeMs  = gev.time,
            .fingers = fingers,
            .delta   = delta,
        };

        CTrackpadGestures* handler = g_pShimTrackpadGestures->get(gev.type);
        handler->gestureBegin(swipeBegin);
        handler->gestureUpdate(swipe);
    }
    this->emulatedSwipePoint = this->m_sGestureState.get_center().current;

    this->activeTrackpadGesture = foundLongPress || handler->m_activeGesture ? handler : nullptr;
    return this->activeTrackpadGesture;
}

void GestureManager::trackpadGestureUpdate(uint32_t time) {
    if (!this->activeTrackpadGesture)
        return;

    const auto currentPoint = this->m_sGestureState.get_center().current;
    const auto deltaPx      = currentPoint - this->emulatedSwipePoint;
    const Vector2D delta    = pixelToTrackpadDistance(deltaPx);

    DragGestureEvent activeDrag = this->getActiveDragGesture().value();
    uint32_t fingers            = activeDrag.type == GestureType::EDGE_SWIPE
                                      ? (activeDrag.edge_origin << MOD_MASK_SHIFT) | (activeDrag.finger_count & FINGERS_MASK)
                                      : activeDrag.finger_count;

    this->emulatedSwipePoint = currentPoint;

    if (activeDrag.type == GestureType::PINCH) {
        IPointer::SPinchUpdateEvent pinch = {
            .timeMs  = time,
            .fingers = fingers,
            .delta   = delta,
            .scale   = this->m_sGestureState.get_pinch_scale(),
            // FIXME: rotation should be relative to previous update event, not the initial one
            .rotation = this->m_sGestureState.get_rotation_angle(),
        };

        this->activeTrackpadGesture->gestureUpdate(pinch);
    } else {
        IPointer::SSwipeUpdateEvent swipe = {
            .timeMs  = time,
            .fingers = fingers,
            .delta   = delta,
        };

        this->activeTrackpadGesture->gestureUpdate(swipe);
    }
}

void GestureManager::trackpadGestureEnd(uint32_t time) {
    DragGestureEvent activeDrag = this->getActiveDragGesture().value();
    if (activeDrag.type == GestureType::PINCH) {
        IPointer::SPinchEndEvent swipe = {
            .timeMs    = time,
            .cancelled = false,
        };
        this->activeTrackpadGesture->gestureEnd(swipe);
    } else {
        IPointer::SSwipeEndEvent swipe = {
            .timeMs    = time,
            .cancelled = false,
        };
        this->activeTrackpadGesture->gestureEnd(swipe);
    }
    this->activeTrackpadGesture = nullptr;
}

void GestureManager::updateLongPressTimer(uint32_t current_time, uint32_t delay) {
    this->long_press_next_trigger_time = current_time + delay + 1;
    wl_event_source_timer_update(this->long_press_timer, delay);
}

void GestureManager::stopLongPressTimer() {
    wl_event_source_timer_update(this->long_press_timer, 0);
}

void GestureManager::sendCancelEventsToWindows() {
    static auto const SEND_CANCEL = g_config->sendCancel;

    // Nothing of this sequence reached a window, and the resources may still list an earlier one.
    if (!SEND_CANCEL->value() || !this->touchesDelivered) {
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
    static auto const SEND_CANCEL = g_config->sendCancel;

    // Held touches replayed to the window: already part of the gesture state.
    if (this->replayingHeldTouches) {
        return false;
    }

    auto monitor =
        State::monitorState()->query().name(!ev.device->m_boundOutput.empty() ? ev.device->m_boundOutput : "").run();
    monitor = monitor ? monitor : Desktop::focusState()->monitor();

    if (!monitor) {
        LOG(Log::ERR, "[hyprgrass] onTouchDown: could not find a monitor???");
        return false;
    }

    this->m_lastTouchedMonitor = monitor;

    const auto& monitorPos  = this->m_lastTouchedMonitor->m_position;
    const auto& monitorSize = this->m_lastTouchedMonitor->m_size;
    this->m_monitorArea     = SMonitorArea{monitorPos.x, monitorPos.y, monitorSize.x, monitorSize.y};

    Pointer::pointerController()->warpTo(
        Vector2D{
            monitorPos.x + ev.pos.x * monitorSize.x,
            monitorPos.y + ev.pos.y * monitorSize.y,
        }
    );

    g_pInputManager->refocus();

    const bool first = this->m_sGestureState.fingers.size() == 0;
    if (first) {
        this->touchedResources.clear();
        this->activeTrackpadGesture = nullptr;
    }

    if (!eventForwardingInhibited() && SEND_CANCEL->value() && g_pInputManager->m_touchData.touchFocusSurface) {
        // remember which surfaces were touched, to later send cancel events
        const auto surface = g_pInputManager->m_touchData.touchFocusSurface;

        wl_client* client = surface.get()->client();
        if (client) {
            SP<CWLSeatResource> seat = g_pSeatManager->seatResourceForClient(client);

            if (seat) {
                auto touches = seat.get()->m_touches;
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

    const bool block = IGestureManager::onTouchDown(gesture_event);
    return this->holdTouchDown(ev, first, block);
}

bool GestureManager::onTouchUp(ITouch::SUpEvent ev) {
    if (this->replayingHeldTouches) {
        return false;
    }

    // While touches are held, or stand in for a pointer, the compositor still points at the surface of an
    // earlier sequence, which forwardTouchUp checks: only a recognised gesture decides.
    const bool undelivered = this->holdingTouches || this->emulatingPointer;
    const bool block       = this->forwardTouchUp(ev);
    return this->holdTouchUp(ev.timeMs, undelivered ? this->eventForwardingInhibited() : block);
}

bool GestureManager::forwardTouchUp(ITouch::SUpEvent ev) {
    static auto const SEND_CANCEL = g_config->sendCancel;

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
    if (SEND_CANCEL->value()) {
        const auto surface = g_pInputManager->m_touchData.touchFocusSurface;

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

        auto touches = seat.get()->m_touches;
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
    if (this->replayingHeldTouches) {
        return false;
    }

    if (!this->m_lastTouchedMonitor) {
        LOG(Log::ERR, "[hyprgrass] onTouchMove: where the fuck is my monitor");
        return false;
    }

    auto pos = wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);

    const wf::touch::gesture_event_t gesture_event = {
        .type   = wf::touch::EVENT_TYPE_MOTION,
        .time   = ev.timeMs,
        .finger = ev.touchID,
        .pos    = pos,
    };

    const bool block = IGestureManager::onTouchMove(gesture_event) != FindGestureResult::NONE;
    return this->holdTouchMove(ev, block);
}

// Touches on shell surfaces (Overlay and Top layers: keyboard, dock, bar, panels) and on the lock screen
// do not wait: they handle a cancel themselves and keep their latency. Hit-tested at the touch point, since
// the pointer focus may still describe an earlier position.
bool GestureManager::shouldHoldTouches(const Vector2D& pos) const {
    if (g_pSessionLockManager->isSessionLocked() || !this->m_lastTouchedMonitor) {
        return false;
    }

    const auto& hit = Desktop::viewState()->hitTest();
    Vector2D local;
    PHLLS found;
    if (hit.layerPopupSurfaceAt(pos, this->m_lastTouchedMonitor, &local, &found)) {
        return false;
    }

    auto& layers = this->m_lastTouchedMonitor->m_layerSurfaceLayers;
    for (const auto layer : {ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, ZWLR_LAYER_SHELL_V1_LAYER_TOP}) {
        if (hit.layerSurfaceAt(pos, &layers[layer], &local, &found)) {
            return false;
        }
    }

    return true;
}

bool GestureManager::clientIgnoresTouch(const Vector2D& pos) const {
    const auto window = Desktop::viewState()->hitTest().windowAt(
        pos, Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING
    );
    const auto surface = window ? window->wlSurface()->resource() : nullptr;
    const auto seat    = surface ? g_pSeatManager->seatResourceForClient(surface->client()) : nullptr;

    return seat && seat->m_touches.empty();
}

// Content follows the finger, as on any touch surface.
void GestureManager::scrollPointer(uint32_t time, const wf::touch::point_t& to) {
    g_pSeatManager->sendPointerAxis(
        time, WL_POINTER_AXIS_VERTICAL_SCROLL, this->scrollPoint.y - to.y, 0, 0, WL_POINTER_AXIS_SOURCE_FINGER,
        WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL
    );
    g_pSeatManager->sendPointerFrame();
    this->scrollPoint = to;
}

// The pointer is already there: every touch warps it.
void GestureManager::clickPointer(uint32_t time) {
    g_pSeatManager->sendPointerButton(time, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    g_pSeatManager->sendPointerFrame();
    g_pSeatManager->sendPointerButton(time, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
    g_pSeatManager->sendPointerFrame();
}

// @return whether this touch down should be blocked from forwarding to the client
bool GestureManager::holdTouchDown(const ITouch::SDownEvent& ev, bool first, bool block) {
    static auto const HOLD_TOUCHES     = g_config->holdTouches;
    static auto const TOUCH_AS_POINTER = g_config->touchAsPointer;

    if (!HOLD_TOUCHES->value()) {
        this->touchesDelivered = true;
        return block;
    }

    if (first) {
        this->heldTouches.clear();
        this->holdExtended   = false;
        this->touchesClaimed = false;
        const auto monitor   = this->m_lastTouchedMonitor;
        const auto global    = monitor ? monitor->m_position + ev.pos * monitor->m_size : Vector2D{};
        this->holdingTouches = !block && this->shouldHoldTouches(global);
        this->touchesDelivered = !this->holdingTouches;
        this->scrolling        = false;
        this->emulatingPointer = TOUCH_AS_POINTER->value() && this->holdingTouches && this->clientIgnoresTouch(global);
        if (!this->holdingTouches) {
            return block;
        }

        this->scrollPoint = this->wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);
        this->heldTouches.push_back(ev);
        wl_event_source_timer_update(this->holdTimer, HOLD_MS);
        return true;
    }

    // A second finger is a gesture, or a pinch for a client that cannot see it anyway.
    this->emulatingPointer = false;

    if (this->touchesClaimed) {
        return true;
    }

    // Three fingers are a gesture: the window never gets them, or gets a cancel for those it already has.
    if (block || this->m_sGestureState.fingers.size() >= 3) {
        this->claimTouches();
        return true;
    }

    if (this->holdingTouches) {
        this->heldTouches.push_back(ev);
        return true;
    }

    return false;
}

// @return whether this touch motion should be blocked from forwarding to the client
bool GestureManager::holdTouchMove(const ITouch::SMotionEvent& ev, bool block) {
    if (this->touchesClaimed) {
        return true;
    }

    if (this->emulatingPointer) {
        const auto to = this->wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);
        if (this->scrolling) {
            this->scrollPointer(ev.timeMs, to);
        } else if (std::hypot(to.x - this->scrollPoint.x, to.y - this->scrollPoint.y) >= HOLD_SLOP) {
            // From here on, so the content does not jump by the slop.
            this->scrolling   = true;
            this->scrollPoint = to;
        }
        return true;
    }

    if (!this->holdingTouches) {
        return block;
    }

    if (block) {
        this->claimTouches();
        return true;
    }

    const auto held =
        std::ranges::find_if(this->heldTouches, [&](const auto& down) { return down.touchID == ev.touchID; });
    if (held == this->heldTouches.end()) {
        return true;
    }

    const auto from = this->wlrTouchEventPositionAsPixels(held->pos.x, held->pos.y);
    const auto to   = this->wlrTouchEventPositionAsPixels(ev.pos.x, ev.pos.y);
    if (std::hypot(to.x - from.x, to.y - from.y) < HOLD_SLOP) {
        return true;
    }

    this->releaseHeldTouches();
    return false;
}

// @return whether this touch up should be blocked from forwarding to the client
bool GestureManager::holdTouchUp(uint32_t time, bool block) {
    bool result = block;
    if (this->touchesClaimed) {
        result = true;
    } else if (this->emulatingPointer) {
        if (!this->scrolling && !block) {
            this->clickPointer(time);
        }
        this->emulatingPointer = false;
        this->holdingTouches   = false;
        result                 = true;
    } else if (this->holdingTouches) {
        if (block) {
            this->claimTouches();
            result = true;
        } else {
            // A lift ends a tap or a short touch: the window gets it whole.
            this->releaseHeldTouches();
            result = false;
        }
    }

    if (this->m_sGestureState.fingers.empty()) {
        wl_event_source_timer_update(this->holdTimer, 0);
        this->heldTouches.clear();
        this->holdingTouches = false;
        this->touchesClaimed = false;
    }

    return result;
}

void GestureManager::onHoldTimeout() {
    static auto const LONG_PRESS_DELAY = g_config->longPressDelay;

    if (!this->holdingTouches) {
        return;
    }

    if (this->eventForwardingInhibited()) {
        this->claimTouches();
        return;
    }

    // Nothing to replay to a client that takes no touch.
    if (this->emulatingPointer) {
        this->heldTouches.clear();
        this->holdingTouches = false;
        return;
    }

    // Two fingers wait for a pinch or scroll (movement), a lift or the two-finger long press; past the long
    // press delay they belong to the window after all.
    if (this->m_sGestureState.fingers.size() >= 2 && !this->holdExtended) {
        this->holdExtended = true;
        const auto rest = std::max<int64_t>(1, *LONG_PRESS_DELAY->m_val.ptr() + 100 - HOLD_MS);
        wl_event_source_timer_update(this->holdTimer, rest);
        return;
    }

    this->releaseHeldTouches();
}

// The rest of the sequence belongs to the compositor; touches already delivered are cancelled.
void GestureManager::claimTouches() {
    wl_event_source_timer_update(this->holdTimer, 0);
    this->heldTouches.clear();
    this->holdingTouches = false;
    this->touchesClaimed = true;
    this->sendCancelEventsToWindows();
}

void GestureManager::releaseHeldTouches() {
    wl_event_source_timer_update(this->holdTimer, 0);
    this->holdingTouches   = false;
    this->touchesDelivered = true;

    const auto held            = std::move(this->heldTouches);
    this->heldTouches          = {};
    this->replayingHeldTouches = true;
    for (const auto& down : held) {
        g_pInputManager->onTouchDown(down);
    }
    this->replayingHeldTouches = false;

    if (held.empty()) {
        return;
    }

    g_pSeatManager->sendTouchFrame();

    // A gesture recognised later (a two-finger long press) cancels the window like any touched surface.
    const auto surface = g_pInputManager->m_touchData.touchFocusSurface;
    if (!surface || !surface->client()) {
        return;
    }

    if (const auto seat = g_pSeatManager->seatResourceForClient(surface->client())) {
        for (const auto& touch : seat->m_touches) {
            this->touchedResources.insert(touch);
        }
    }
}

SMonitorArea GestureManager::getMonitorArea() const {
    return this->m_monitorArea;
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

Vector2D GestureManager::pixelToTrackpadDistance(wf::touch::point_t distancePx) const {
    static auto const PSWIPEDIST = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    const auto SWIPEDISTANCE     = std::clamp(*PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);

    const auto monArea       = this->getMonitorArea();
    const auto delta_percent = distancePx / wf::touch::point_t(monArea.w, monArea.h);

    return Vector2D(delta_percent.x * SWIPEDISTANCE, delta_percent.y * SWIPEDISTANCE);
}

void GestureManager::debugLog(const std::string& msg) {
    LOG(Log::DEBUG, "[hyprgrass] " + msg);
}

void hyprgrass_debug(const std::string& s) {
    LOG(Log::DEBUG, "[hyprgrass] [debug] {}", s);
}
