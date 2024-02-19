#include "GestureManager.hpp"
#include "globals.hpp"

#include <algorithm>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <hyprlang.hpp>
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                                Hyprlang::CConfigValue((Hyprlang::INT)3));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_edge",
                                Hyprlang::CConfigValue((Hyprlang::STRING) "d"));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity",
                                Hyprlang::CConfigValue((Hyprlang::FLOAT)1.0));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:long_press_delay",
                                Hyprlang::CConfigValue((Hyprlang::INT)400));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel",
                                Hyprlang::CConfigValue((Hyprlang::INT)0));
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

    return {"hyprgrass", "Touchscreen gestures", "horriblename", "0.5"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
