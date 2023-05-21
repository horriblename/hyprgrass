#include "GestureManager.hpp"
#include "globals.hpp"

#include <algorithm>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <vector>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f,
                              1.0f};
inline CFunctionHook* g_pTouchDownHook = nullptr;
inline CFunctionHook* g_pTouchUpHook   = nullptr;
inline CFunctionHook* g_pTouchMoveHook = nullptr;
typedef void (*origTouchDown)(void*, wlr_touch_down_event*);
typedef void (*origTouchUp)(void*, wlr_touch_up_event*);
typedef void (*origTouchMove)(void*, wlr_touch_motion_event*);

// Inhibit all calls to Hyprland's original touch event handlers.
inline bool g_bInhibitHyprlandTouchHandlers = false;
inline std::vector<wlr_surface*> g_vTouchedSurfaces;

inline void markSurfaceAsTouched(wlr_surface& surface) {
    const auto TOUCHED = std::find(g_vTouchedSurfaces.begin(),
                                   g_vTouchedSurfaces.end(), &surface);
    if (TOUCHED == g_vTouchedSurfaces.end()) {
        g_vTouchedSurfaces.push_back(&surface);
    }
}

inline void cancelAllFingers() {
    for (const auto& window : g_vTouchedSurfaces) {
        if (!window) {
            continue;
        }

        wlr_seat_touch_notify_cancel(g_pCompositor->m_sSeat.seat, window);
    }

    g_vTouchedSurfaces.clear();
    g_bInhibitHyprlandTouchHandlers = true;
}

void hkOnTouchDown(void* thisptr, wlr_touch_down_event* e) {
    if (e->touch_id == 0) {
        g_bInhibitHyprlandTouchHandlers = false;
        g_vTouchedSurfaces.clear();
    }

    const auto BLOCK = g_pGestureManager->onTouchDown(e);

    if (g_bInhibitHyprlandTouchHandlers) {
        return;
    }

    if (BLOCK) {
        cancelAllFingers();
        return;
    }

    (*(origTouchDown)g_pTouchDownHook->m_pOriginal)(thisptr, e);

    if (g_pInputManager->m_sTouchData.touchFocusSurface) {
        markSurfaceAsTouched(*g_pInputManager->m_sTouchData.touchFocusSurface);
    }
}

void hkOnTouchUp(void* thisptr, wlr_touch_up_event* e) {
    const auto BLOCK = g_pGestureManager->onTouchUp(e);

    if (g_bInhibitHyprlandTouchHandlers) {
        return;
    }

    if (BLOCK) {
        cancelAllFingers();
        // e->finger_id is not handled by liftAllFingers(), do not return
    }

    (*(origTouchUp)g_pTouchUpHook->m_pOriginal)(thisptr, e);
}

void hkOnTouchMove(void* thisptr, wlr_touch_motion_event* e) {
    const auto BLOCK = g_pGestureManager->onTouchMove(e);

    if (g_bInhibitHyprlandTouchHandlers) {
        return;
    }

    if (BLOCK) {
        cancelAllFingers();
        return;
    }

    (*(origTouchMove)g_pTouchMoveHook->m_pOriginal)(thisptr, e);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    bool cfgStatus = true;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    cfgStatus = cfgStatus &&
                HyprlandAPI::addConfigValue(
                    PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                    SConfigValue{.intValue = 3});
    cfgStatus = cfgStatus && HyprlandAPI::addConfigValue(
                                 PHANDLE, "plugin:touch_gestures:sensitivity",
                                 SConfigValue{.floatValue = 1.0});

    if (!cfgStatus) {
        HyprlandAPI::addNotification(PHANDLE,
                                     "config values cannot be properly added",
                                     CColor(0.8, 0.2, 0.2, 1.0), 5000);
    }
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
    g_pTouchDownHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchDown, (void*)&hkOnTouchDown);
    g_pTouchUpHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchUp, (void*)&hkOnTouchUp);
    g_pTouchMoveHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchMove, (void*)&hkOnTouchMove);
#pragma GCC diagnostic pop

    g_pTouchDownHook->hook();
    g_pTouchUpHook->hook();
    g_pTouchMoveHook->hook();

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<CGestures>();

    return {"touch-gestures", "Touchscreen gestures", "horriblename", "0.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
