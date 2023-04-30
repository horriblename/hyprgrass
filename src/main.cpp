// #include "src/plugins/PluginAPI.hpp"
#include "src/GestureManager.hpp"
#include <memory>
#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <src/config/ConfigManager.hpp>
#include <src/managers/input/InputManager.hpp>
// #include <wlr/types/wlr_touch.h>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f,
                              1.0f};
inline CFunctionHook* g_pTouchDownHook = nullptr;
inline CFunctionHook* g_pTouchUpHook   = nullptr;
inline CFunctionHook* g_pTouchMoveHook = nullptr;
typedef void (*origTouchDown)(void*, wlr_touch_down_event*);
typedef void (*origTouchUp)(void*, wlr_touch_up_event*);
typedef void (*origTouchMove)(void*, wlr_touch_motion_event*);

void hkOnTouchDown(void* thisptr, wlr_touch_down_event* e) {
    if (g_pGestureManager->onTouchDown(e))
        return;

    (*(origTouchDown)g_pTouchDownHook->m_pOriginal)(thisptr, e);
}

void hkOnTouchUp(void* thisptr, wlr_touch_up_event* e) {
    if (g_pGestureManager->onTouchUp(e))
        return;

    (*(origTouchUp)g_pTouchUpHook->m_pOriginal)(thisptr, e);
}

void hkOnTouchMove(void* thisptr, wlr_touch_motion_event* e) {
    if (g_pGestureManager->onTouchMove(e))
        return;

    (*(origTouchMove)g_pTouchMoveHook->m_pOriginal)(thisptr, e);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    bool cfgStatus;
    cfgStatus = cfgStatus &&
                HyprlandAPI::addConfigValue(
                    PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                    SConfigValue{.intValue = 3});

    if (!cfgStatus) {
        HyprlandAPI::addNotification(PHANDLE,
                                     "config values cannot be properly added",
                                     CColor(0.8, 0.2, 0.2, 1.0), 5000);
    }


    // Hook a private member
    g_pTouchDownHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchDown, (void*)&hkOnTouchDown);
    g_pTouchUpHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchUp, (void*)&hkOnTouchUp);
    g_pTouchMoveHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchMove, (void*)&hkOnTouchMove);

    g_pGestureManager = std::make_unique<CGestures>();

    g_pTouchDownHook->hook();
    g_pTouchUpHook->hook();
    g_pTouchMoveHook->hook();

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE,
                                 "[touch-gestures] Initialized successfully!",
                                 s_pluginColor, 5000);

    return {"touch-gestures", "Touchscreen gestures", "horriblename", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
    HyprlandAPI::addNotification(PHANDLE,
                                 "[touch-gestures] Unloaded successfully!",
                                 s_pluginColor, 5000);
}
