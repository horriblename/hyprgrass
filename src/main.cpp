// #include "src/plugins/PluginAPI.hpp"
#include "src/gestures.hpp"
#include <memory>
#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <src/config/ConfigManager.hpp>
#include <src/managers/input/InputManager.hpp>
// #include <wlr/types/wlr_touch.h>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f,
                              1.0f};
inline CFunctionHook* g_pTouchDownHook = nullptr;
inline CFunctionHook* g_pTouchUpHook = nullptr;
inline CFunctionHook* g_pTouchMoveHook = nullptr;

void hkOnTouchDown(void* thisptr, wlr_touch_down_event* e) {
    if (g_pGestureManager->onTouchDown(e))
        return;

    g_pInputManager->onTouchDown(e);
}

void hkOnTouchUp(void* thisptr, wlr_touch_up_event* e) {
    g_pInputManager->onTouchUp(e);
}

void hkOnTouchMove(void* thisptr, wlr_touch_motion_event* e) {
    g_pInputManager->onTouchMove(e);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "touch-gestures:3:left-right",
                                SConfigValue{.strValue = ""});
    HyprlandAPI::addConfigValue(PHANDLE, "touch-gestures:3:right-left",
                                SConfigValue{.strValue = ""});

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE,
                                 "[touch-gestures] Initialized successfully!",
                                 s_pluginColor, 5000);

    // Hook a private member
    g_pTouchDownHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchDown, (void*)&hkOnTouchDown);
    g_pTouchUpHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchUp, (void*)&hkOnTouchUp);
    g_pTouchMoveHook = HyprlandAPI::createFunctionHook(
        PHANDLE, (void*)&CInputManager::onTouchDown, (void*)&hkOnTouchMove);

    g_pGestureManager = std::make_unique<CGestures>();

    return {"touch-gestures", "Touchscreen gestures", "horriblename", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::addNotification(PHANDLE,
                                 "[touch-gestures] Unloaded successfully!",
                                 s_pluginColor, 5000);
}
