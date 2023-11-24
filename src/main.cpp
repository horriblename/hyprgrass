#include "GestureManager.hpp"
#include "globals.hpp"

#include <algorithm>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <vector>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

void hkOnTouchDown(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<wlr_touch_down_event*>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchDown(ev);
}

void hkOnTouchUp(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<wlr_touch_up_event*>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchUp(ev);
}

void hkOnTouchMove(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<wlr_touch_motion_event*>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchMove(ev);
}

HOOK_CALLBACK_FN gTouchDownCallback = hkOnTouchDown;
HOOK_CALLBACK_FN gTouchUpCallback   = hkOnTouchUp;
HOOK_CALLBACK_FN gTouchMoveCallback = hkOnTouchMove;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    bool cfgStatus = true;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    cfgStatus = cfgStatus && HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                                                         SConfigValue{.intValue = 3});
    cfgStatus = cfgStatus && HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity",
                                                         SConfigValue{.floatValue = 1.0});
    cfgStatus = cfgStatus && HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel",
                                                         SConfigValue{.intValue = 0});

    if (!cfgStatus) {
        HyprlandAPI::addNotification(PHANDLE, "config values cannot be properly added", CColor(0.8, 0.2, 0.2, 1.0),
                                     5000);
    }
#pragma GCC diagnostic pop

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CColor(0.8, 0.2, 0.2, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    HyprlandAPI::registerCallbackStatic(PHANDLE, "touchDown", &gTouchDownCallback);
    HyprlandAPI::registerCallbackStatic(PHANDLE, "touchUp", &gTouchUpCallback);
    HyprlandAPI::registerCallbackStatic(PHANDLE, "touchMove", &gTouchMoveCallback);

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<GestureManager>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", "0.4"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
