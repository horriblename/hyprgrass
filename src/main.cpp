#include "GestureManager.hpp"
#include "globals.hpp"

#include <algorithm>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/version.h>
#include <vector>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f,
                              1.0f};
inline CFunctionHook* g_pTouchDownHook = nullptr;
inline CFunctionHook* g_pTouchUpHook   = nullptr;
inline CFunctionHook* g_pTouchMoveHook = nullptr;
typedef void (*origTouchDown)(void*, wlr_touch_down_event*);
typedef void (*origTouchUp)(void*, wlr_touch_up_event*);
typedef void (*origTouchMove)(void*, wlr_touch_motion_event*);

void hkOnTouchDown(void* thisptr, wlr_touch_down_event* e) {
    const auto BLOCK = g_pGestureManager->onTouchDown(e);

    if (BLOCK) {
        return;
    }

    (*(origTouchDown)g_pTouchDownHook->m_pOriginal)(thisptr, e);
}

void hkOnTouchUp(void* thisptr, wlr_touch_up_event* e) {
    const auto BLOCK = g_pGestureManager->onTouchUp(e);

    if (BLOCK) {
        return;
    }

    (*(origTouchUp)g_pTouchUpHook->m_pOriginal)(thisptr, e);
}

void hkOnTouchMove(void* thisptr, wlr_touch_motion_event* e) {
    const auto BLOCK = g_pGestureManager->onTouchMove(e);

    if (BLOCK) {
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

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(
            PHANDLE, "Mismatched Hyprland version! check logs for details",
            CColor(0.8, 0.2, 0.2, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}",
                   hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}",
                   hlVersion.hash);
    }

    g_pTouchDownHook->hook();
    g_pTouchUpHook->hook();
    g_pTouchMoveHook->hook();

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<CGestures>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", "0.4"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
