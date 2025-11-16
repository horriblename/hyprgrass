#include "GestureManager.hpp"
#include "HyprLogger.hpp"
#include "globals.hpp"

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/UnifiedWorkspaceSwipeGesture.hpp>
#include <hyprland/src/plugins/PluginSystem.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#undef private

#include <algorithm>
#include <ranges>
#include <vector>

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

struct HookResult {
    HANDLE handled;
    bool needsDeadCleanup = false;
};

static HookResult emitOneHook(const SCallbackFNPtr& cb, std::any data, std::vector<HANDLE>& faultyHandles) {
    SCallbackInfo info = {};

    if (!cb.handle) {
        // hyprland hooks, impossible to occur but we're skipping those just in case
        return HookResult{.handled = nullptr};
    }

    if (std::ranges::find(faultyHandles, cb.handle) != faultyHandles.end())
        return HookResult{.handled = nullptr};

    g_pHookSystem->m_currentEventPlugin = true;

    try {
        if (!setjmp(g_pHookSystem->m_hookFaultJumpBuf)) {
            if (SP<HOOK_CALLBACK_FN> fn = cb.fn.lock()) {
                (*fn)(fn.get(), info, data);
                if (info.cancelled)
                    return HookResult{
                        .handled          = cb.handle,
                        .needsDeadCleanup = false,
                    };
            } else {
                return HookResult{.handled = nullptr, .needsDeadCleanup = true};
            }
        } else {
            // this module crashed.
            throw std::exception();
        }
    } catch (std::exception& e) {
        // TODO: this works only once...?
        faultyHandles.push_back(cb.handle);
        Debug::log(
            ERR, "[hookSystem] Hook from plugin {:x} caused a SIGSEGV, queueing for unloading.",
            rc<uintptr_t>(cb.handle)
        );
    }

    return HookResult{.handled = nullptr, .needsDeadCleanup = false};
}

// copied from g_pHookSystem->emit(), emits events for each hook until one of them sets
// info.cancelled=true; returns the handle of the plugin that handled the event.
static HANDLE emitHookEventForOne(std::vector<SCallbackFNPtr>* const callbacks, std::any data) {
    if (callbacks->empty())
        return nullptr;

    HANDLE handler = nullptr;

    std::vector<HANDLE> faultyHandles;
    volatile bool needsDeadCleanup = false;

    for (auto const& cb : *callbacks) {
        HookResult res = emitOneHook(cb, data, faultyHandles);
        if (res.needsDeadCleanup)
            needsDeadCleanup = true;
        if (res.handled) {
            handler = res.handled;
            break;
        }
    }

    if (needsDeadCleanup)
        std::erase_if(*callbacks, [](const auto& fn) { return !fn.fn.lock(); });

    if (!faultyHandles.empty()) {
        for (auto const& h : faultyHandles)
            g_pPluginSystem->unloadPlugin(g_pPluginSystem->getPluginByHandle(h), true);
    }

    return handler;
}

#define EMIT_HOOK_EVENT_FOR_PLUGIN(name, plugin, param)                                                                \
    {                                                                                                                  \
        static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent(name);                                            \
        emitHookForPlugin(PEVENTVEC, plugin, param);                                                                   \
    }

static void emitHookForPlugin(std::vector<SCallbackFNPtr>* const callbacks, HANDLE plugin, std::any data) {
    if (callbacks->empty())
        return;

    std::vector<HANDLE> faultyHandles;

    volatile bool needsDeadCleanup = false;
    for (auto const& cb : *callbacks) {
        if (cb.handle == plugin) {
            HookResult res = emitOneHook(cb, data, faultyHandles);
            if (res.needsDeadCleanup)
                needsDeadCleanup = true;
            break;
        }
    }

    if (needsDeadCleanup)
        std::erase_if(*callbacks, [](const auto& fn) { return !fn.fn.lock(); });

    if (!faultyHandles.empty()) {
        for (auto const& h : faultyHandles)
            g_pPluginSystem->unloadPlugin(g_pPluginSystem->getPluginByHandle(h), true);
    }

    return;
}

int handleLongPressTimer(void* data) {
    const auto gesture_manager = (GestureManager*)data;
    gesture_manager->onLongPressTimeout(gesture_manager->long_press_next_trigger_time);

    return 0;
}

std::string commaSeparatedCssGaps(CCssGapData data) {
    return std::to_string(data.m_top) + "," + std::to_string(data.m_right) + "," + std::to_string(data.m_bottom) + "," +
           std::to_string(data.m_left);
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

    this->addEdgeSwipeGesture(*PSENSITIVITY, *LONG_PRESS_DELAY, *EDGE_MARGIN);
    this->addLongPress(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addMultiFingerGesture(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addMultiFingerTap(*PSENSITIVITY, *LONG_PRESS_DELAY);
    this->addPinchGesture(*PSENSITIVITY, *LONG_PRESS_DELAY);

    this->long_press_timer = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, handleLongPressTimer, this);
}

GestureManager::~GestureManager() {
    wl_event_source_remove(this->long_press_timer);
}

bool GestureManager::findCompletedGesture(const CompletedGestureEvent& gev) const {
    return this->findGestureBind(gev.to_string(), GestureEventType::COMPLETED);
}
bool GestureManager::handleCompletedGesture(const CompletedGestureEvent& gev) {
    return this->handleGestureBind(gev.to_string(), GestureEventType::COMPLETED);
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
    static auto const EMULATE_TOUCHPAD =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:emulate_touchpad_swipe")
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
            static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent("hyprgrass:swipeBegin");

            HANDLE handler = emitHookEventForOne(
                PEVENTVEC,
                std::tuple<std::string, std::uint64_t, Vector2D>{
                    stringifyDirection(gev.direction), gev.finger_count,
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().origin)
                }
            );

            if (handler) {
                this->hookHandled = handler;
                return true;
            }

            if (this->trackpadGestureBegin(gev))
                return true;

            if (**WORKSPACE_SWIPE_FINGERS != gev.finger_count) {
                return false;
            }

            if (this->handleWorkspaceSwipe(gev.direction))
                return true;

            if (!**EMULATE_TOUCHPAD)
                return false;

            this->workspaceSwipeActive       = false; // reset just in case
            this->emulatedSwipePoint         = this->m_sGestureState.get_center().current;
            IPointer::SSwipeBeginEvent swipe = {.fingers = static_cast<uint32_t>(m_sGestureState.fingers.size())};
            g_pInputManager->onSwipeBegin(swipe);
            return true;
        }

        case DragGestureType::EDGE_SWIPE: {
            static auto* const PEVENTVEC = g_pHookSystem->getVecForEvent("hyprgrass:edgeBegin");

            HANDLE handler = emitHookEventForOne(
                PEVENTVEC,
                std::pair<std::string, Vector2D>(
                    gev.to_string(), pixelPositionToPercentagePosition(this->m_sGestureState.get_center().origin)
                )
            );

            if (handler) {
                this->hookHandled = handler;
                return true;
            }

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

        case DragGestureType::LONG_PRESS:
            if (g_pSessionLockManager->isSessionLocked()) {
                return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN);
            }

            if (**RESIZE_LONG_PRESS && gev.finger_count == 1) {
                const auto BORDER_GRAB_AREA = **PBORDERSIZE + **PBORDERGRABEXTEND;

                // kind of a hack: this is the window detected from previous touch events
                const auto w = g_pInputManager->m_foundWindowToFocus.lock();
                const Vector2D touchPos =
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().current) *
                    this->m_lastTouchedMonitor->m_size;
                if (w && !w->isFullscreen()) {
                    const CBox real = {
                        w->m_realPosition->value().x, w->m_realPosition->value().y, w->m_realSize->value().x,
                        w->m_realSize->value().y
                    };
                    const CBox grab = {
                        real.x - BORDER_GRAB_AREA, real.y - BORDER_GRAB_AREA, real.width + 2 * BORDER_GRAB_AREA,
                        real.height + 2 * BORDER_GRAB_AREA
                    };

                    bool notInRealWindow = !real.containsPoint(touchPos) || w->isInCurvedCorner(touchPos.x, touchPos.y);
                    bool onTiledGap      = !w->m_isFloating && !w->isFullscreen() && notInRealWindow;
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
                        newGapsIn.m_top += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_right += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_bottom += RESIZE_BORDER_GAP_INCREMENT;
                        newGapsIn.m_left += RESIZE_BORDER_GAP_INCREMENT;
                        g_pConfigManager->parseKeyword("general:gaps_in", commaSeparatedCssGaps(newGapsIn));
                        return true;
                    }
                }
            }

            if (this->trackpadGestureBegin(gev))
                return true;

            return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN);

        case DragGestureType::PINCH:
            if (this->trackpadGestureBegin(gev))
                return true;

            return this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_BEGIN);
            break;
    }

    return false;
}

bool GestureManager::findGestureBind(std::string bind, GestureEventType type) const {
    Debug::log(LOG, "[hyprgrass] Looking for binds matching: {}", bind);

    auto allBinds   = std::ranges::views::join(std::array{g_pKeybindManager->m_keybinds, this->internalBinds});
    const auto MODS = g_pInputManager->getModsFromAllKBs();

    for (const auto& k : allBinds) {
        if (k->key != bind)
            continue;

        if (k->handler == "pass")
            continue;

        if (k->locked != g_pSessionLockManager->isSessionLocked())
            continue;

        if (k->modmask != MODS)
            continue;

        return true;
    }
    return false;
}

// bind is the name of the gesture event.
// pressed only matters for mouse binds: only start of drag gestures should set it to true
bool GestureManager::handleGestureBind(std::string bind, GestureEventType type) {
    bool found = false;
    Debug::log(LOG, "[hyprgrass] Looking for binds matching: {}", bind);

    auto allBinds   = std::ranges::views::join(std::array{g_pKeybindManager->m_keybinds, this->internalBinds});
    const auto MODS = g_pInputManager->getModsFromAllKBs();

    for (const auto& k : allBinds) {
        if (k->key != bind)
            continue;

        if (k->handler == "pass")
            continue;

        if (k->locked != g_pSessionLockManager->isSessionLocked())
            continue;

        if (k->modmask != MODS)
            continue;

        const auto DISPATCHER = g_pKeybindManager->m_dispatchers.find(k->mouse ? "mouse" : k->handler);

        // Should never happen, as we check in the ConfigManager, but oh well
        if (DISPATCHER == g_pKeybindManager->m_dispatchers.end()) {
            Debug::log(ERR, "Invalid handler in a keybind! (handler {} does not exist)", k->handler);
            continue;
        }

        switch (type) {
            case GestureEventType::COMPLETED:
                if (k->handler != "mouse") {
                    Debug::log(LOG, "[hyprgrass] calling dispatcher ({})", bind);
                    DISPATCHER->second(k->arg);
                    found = found || !k->nonConsuming;
                }
            default:
                if (k->handler == "mouse") {
                    Debug::log(LOG, "[hyprgrass] calling mouse dispatcher ({})", bind);
                    char pressed = type == GestureEventType::DRAG_BEGIN ? '1' : '0';
                    DISPATCHER->second(pressed + k->arg);
                    found = found || !k->nonConsuming;
                }
        }
    }

    return found;
}

void GestureManager::handleCancelledGesture() {}

void GestureManager::dragGestureUpdate(const wf::touch::gesture_event_t& ev) {
    static auto const EMULATE_TOUCHPAD =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:emulate_touchpad_swipe")
            ->getDataStaticPtr();

    if (!this->getActiveDragGesture().has_value()) {
        return;
    }

    if (this->activeTrackpadGesture) {
        this->trackpadGestureUpdate(ev.time);
        return;
    }

    switch (this->getActiveDragGesture()->type) {
        case DragGestureType::SWIPE:
            if (this->hookHandled) {
                EMIT_HOOK_EVENT_FOR_PLUGIN(
                    "hyprgrass:swipeUpdate", this->hookHandled,
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().current)
                )
            } else if (this->workspaceSwipeActive) {
                this->updateWorkspaceSwipe();
            } else if (**EMULATE_TOUCHPAD) {
                const auto currentPoint           = this->m_sGestureState.get_center().current;
                const auto delta                  = currentPoint - this->emulatedSwipePoint;
                IPointer::SSwipeUpdateEvent swipe = {
                    .fingers = this->getActiveDragGesture()->finger_count, .delta = Vector2D(delta.x, delta.y)
                };
                g_pInputManager->onSwipeUpdate(swipe);
                this->emulatedSwipePoint = currentPoint;
            };

        case DragGestureType::LONG_PRESS: {
            const auto pos = this->m_sGestureState.get_center().current;
            g_pCompositor->warpCursorTo(Vector2D(pos.x, pos.y));
            g_pInputManager->simulateMouseMovement();
            return;
        }
        case DragGestureType::EDGE_SWIPE:
            if (this->hookHandled) {
                EMIT_HOOK_EVENT_FOR_PLUGIN(
                    "hyprgrass:edgeUpdate", this->hookHandled,
                    pixelPositionToPercentagePosition(this->m_sGestureState.get_center().current)
                )

                return;
            }

            this->updateWorkspaceSwipe();
        case DragGestureType::PINCH:
            break;
    }
}

void GestureManager::handleDragGestureEnd(const DragGestureEvent& gev) {
    static auto const EMULATE_TOUCHPAD =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:emulate_touchpad_swipe")
            ->getDataStaticPtr();

    if (g_pSessionLockManager->isSessionLocked()) {
        this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
        return;
    }

    if (this->activeTrackpadGesture) {
        this->trackpadGestureEnd(gev.time);
        return;
    }

    Debug::log(LOG, "[hyprgrass] Drag gesture ended: {}", gev.to_string());
    switch (gev.type) {
        case DragGestureType::SWIPE:
            if (this->hookHandled) {
                EMIT_HOOK_EVENT_FOR_PLUGIN("hyprgrass:swipeEnd", this->hookHandled, 0)
                this->hookHandled = nullptr;
            } else if (this->workspaceSwipeActive) {
                g_pUnifiedWorkspaceSwipe->end();
                this->workspaceSwipeActive = false;
            } else if (**EMULATE_TOUCHPAD) {
                g_pInputManager->onSwipeEnd(IPointer::SSwipeEndEvent{.cancelled = false});
            }
            return;
        case DragGestureType::LONG_PRESS:
            if (this->resizeOnBorderInfo.active) {
                g_pKeybindManager->changeMouseBindMode(eMouseBindMode::MBIND_INVALID);
                g_pConfigManager->parseKeyword(
                    "general:gaps_in", commaSeparatedCssGaps(this->resizeOnBorderInfo.old_gaps_in)
                );
                this->resizeOnBorderInfo = {};
                return;
            }

            this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
            return;
        case DragGestureType::EDGE_SWIPE:
            if (this->hookHandled) {
                EMIT_HOOK_EVENT_FOR_PLUGIN("hyprgrass:edgeEnd", this->hookHandled, 0);
                this->hookHandled = nullptr;
            } else if (this->workspaceSwipeActive) {
                g_pUnifiedWorkspaceSwipe->end();
            }
            break;
        case DragGestureType::PINCH:
            this->handleGestureBind(gev.to_string(), GestureEventType::DRAG_END);
            return;
    }
}

bool GestureManager::handleWorkspaceSwipe(const GestureDirection direction) {
    const bool VERTANIMS =
        g_pCompositor->m_lastMonitor->m_activeWorkspace->m_renderOffset->getConfig()->pValues->internalStyle ==
            "slidevert" ||
        g_pCompositor->m_lastMonitor->m_activeWorkspace->m_renderOffset->getConfig()
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
    if (gev.type == DragGestureType::LONG_PRESS && std::abs(delta.x) < 5 && std::abs(delta.y) < 5) {
        const auto MODS = g_pInputManager->getModsFromAllKBs();
        for (const auto& g : g_pShimTrackpadGestures->longPress()->m_gestures) {
            if (g->fingerCount == gev.finger_count && g->modMask == MODS) {
                foundLongPress = true;
                break;
            }
        }
    }
    uint32_t fingers = gev.type == DragGestureType::EDGE_SWIPE ? gev.edge_origin : gev.finger_count;

    CTrackpadGestures* handler = g_pShimTrackpadGestures->get(gev.type);
    if (gev.type == DragGestureType::PINCH) {
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
    uint32_t fingers =
        activeDrag.type == DragGestureType::EDGE_SWIPE ? activeDrag.edge_origin : activeDrag.finger_count;

    this->emulatedSwipePoint = currentPoint;

    if (activeDrag.type == DragGestureType::PINCH) {
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
    if (activeDrag.type == DragGestureType::PINCH) {
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

    this->m_lastTouchedMonitor =
        g_pCompositor->getMonitorFromName(!ev.device->m_boundOutput.empty() ? ev.device->m_boundOutput : "");

    this->m_lastTouchedMonitor =
        this->m_lastTouchedMonitor ? this->m_lastTouchedMonitor : g_pCompositor->m_lastMonitor.lock();

    const auto& monitorPos  = this->m_lastTouchedMonitor->m_position;
    const auto& monitorSize = this->m_lastTouchedMonitor->m_size;
    this->m_monitorArea     = {monitorPos.x, monitorPos.y, monitorSize.x, monitorSize.y};

    g_pCompositor->warpCursorTo({
        monitorPos.x + ev.pos.x * monitorSize.x,
        monitorPos.y + ev.pos.y * monitorSize.y,
    });

    g_pInputManager->refocus();

    if (this->m_sGestureState.fingers.size() == 0) {
        this->touchedResources.clear();
        this->hookHandled           = nullptr;
        this->activeTrackpadGesture = nullptr;
    }

    if (!eventForwardingInhibited() && **SEND_CANCEL && g_pInputManager->m_touchData.touchFocusSurface) {
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

    return IGestureManager::onTouchDown(gesture_event);
}

bool GestureManager::onTouchUp(ITouch::SUpEvent ev) {
    static auto const SEND_CANCEL =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel")
            ->getDataStaticPtr();

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
    static auto const PSWIPEDIST =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "gestures:workspace_swipe_distance")
            ->getDataStaticPtr();
    const auto SWIPEDISTANCE = std::clamp(**PSWIPEDIST, (int64_t)1LL, (int64_t)UINT32_MAX);

    const auto monArea       = this->getMonitorArea();
    const auto delta_percent = distancePx / wf::touch::point_t(monArea.w, monArea.h);

    return Vector2D(delta_percent.x * SWIPEDISTANCE, delta_percent.y * SWIPEDISTANCE);
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

void GestureManager::debugLog(const std::string& msg) {
    Debug::log(LOG, "[hyprgrass] " + msg);
}

void hyprgrass_debug(const std::string& s) {
    Debug::log(LOG, "[hyprgrass] [debug] {}", s);
}
