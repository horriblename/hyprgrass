#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <src/config/ConfigManager.hpp>
// #include <wlr/types/wlr_touch.h>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};
inline CFunctionHook* g_pTouchDownHook = nullptr;

void hkOnTouchDown(void* thisptr, wlr_touch_down_event* e) {
    // static auto* const PGESTURELR = &HyprlandAPI::getConfigValue(PHANDLE, "touch-gestures:3:left-right")->strValue;
    // static auto* const PGESTURERL = &HyprlandAPI::getConfigValue(PHANDLE, "touch-gestures:3:right-left")->strValue;
    HyprlandAPI::addNotification(PHANDLE, "got touch down event", s_pluginColor, 5000);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "touch-gestures:3:left-right", SConfigValue{.strValue = ""});
    HyprlandAPI::addConfigValue(PHANDLE, "touch-gestures:3:right-left", SConfigValue{.strValue = ""});

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[touch-gestures] Initialized successfully!", s_pluginColor, 5000);

    // Hook a private member (!WARNING: the signature may differ in clang. This one is for gcc ONLY.)
    auto funcAddr = HyprlandAPI::getFunctionAddressFromSignature(PHANDLE, "_ZN13CInputManager11onTouchDownEP20wlr_touch_down_event");

    // auto funcAddr = HyprlandAPI::getFunctionAddressFromSignature(PHANDLE, "_ZN13CInputManager22processMouseDownNormalEP24wlr_pointer_button_event");

    if (funcAddr) {
        g_pTouchDownHook = HyprlandAPI::createFunctionHook(PHANDLE, funcAddr, (void*)&hkOnTouchDown);
        g_pTouchDownHook->hook();
    } else {
        HyprlandAPI::addNotification(PHANDLE, "Failed to get function address of InputManager::onTouchDown!", s_pluginColor, 5000);
    }

    return {"touch-gestures", "Touchscreen gestures", "horriblename", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::addNotification(PHANDLE, "[split-monitor-workspaces] Unloaded successfully!", s_pluginColor, 5000);
}
