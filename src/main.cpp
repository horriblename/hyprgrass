#include "GestureManager.hpp"
#include "globals.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <string>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

void hkOnTouchDown(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SDownEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchDown(ev);
}

void hkOnTouchUp(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SUpEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchUp(ev);
}

void hkOnTouchMove(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SMotionEvent>(e);

    cbinfo.cancelled = g_pGestureManager->onTouchMove(ev);
}

std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchDownHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchUpHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchMoveHook;

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

    HyprlandAPI::addDispatcher(PHANDLE, "touchBind",
                               [&](std::string args) { g_pGestureManager->touchBindDispatcher(args); });

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CColor(0.8, 0.2, 0.2, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchDown", hkOnTouchDown);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchUp", hkOnTouchUp);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchMove", hkOnTouchMove);

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<GestureManager>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", "0.6"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
