#include "../../src/version.hpp"
#include "Debouncer.hpp"
#include "globals.hpp"
#include "pulse.hpp"
#include "src/SharedDefs.hpp"

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <memory>
#include <string>
#include <utility>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

std::shared_ptr<AudioBackend> g_pAudioBackend = AudioBackend::getInstance();
std::unique_ptr<Debouncer> g_pDebouncer;
struct GlobalState {
    Vector2D last_triggered_pos;
    Vector2D latest_pos;
};
std::unique_ptr<GlobalState> g_pGlobalState = {};

void onEdgeBegin(void*, SCallbackInfo& cbinfo, std::any args) {
    auto ev = std::any_cast<std::pair<std::string, Vector2D>>(args);

    static auto const WORKSPACE_SWIPE_EDGE =
        (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_edge")
            ->getDataStaticPtr();

    if (ev.first != *WORKSPACE_SWIPE_EDGE) {
        return;
    }

    g_pGlobalState->latest_pos         = ev.second;
    g_pGlobalState->last_triggered_pos = ev.second;

    cbinfo.cancelled = true;
}

void onEdgeUpdate(void*, SCallbackInfo& cbinfo, std::any args) {
    auto pos = std::any_cast<Vector2D>(args);

    // TODO respect config
    g_pGlobalState->latest_pos = pos;
}

void onEdgeEnd(void*, SCallbackInfo& cbinfo, std::any args) {
    g_pDebouncer->disarm();
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprgrass-pipewire:edge_event",
                                Hyprlang::CConfigValue((Hyprlang::STRING) "edge:l:u"));

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CColor(0.8, 0.7, 0.26, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    g_pDebouncer = std::make_unique<Debouncer>(g_pCompositor->m_sWLEventLoop, 200, []() {
        if (g_pGlobalState->latest_pos == g_pGlobalState->last_triggered_pos) {
            return;
        }

        // TODO: make configurable
        const double MAX_RANGE = 0.7; // how much percent of the screen to swipe to get from volume 0 to 100

        // TODO: edge orientation
        double delta      = g_pGlobalState->latest_pos.y - g_pGlobalState->last_triggered_pos.y;
        ChangeType change = delta > 0 ? ChangeType::Increase : ChangeType::Decrease;

        g_pAudioBackend->changeVolume(change, delta, MAX_RANGE);

        g_pGlobalState->last_triggered_pos = g_pGlobalState->latest_pos;
    });

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeBegin", onEdgeBegin);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeUpdate", onEdgeUpdate);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeEnd", onEdgeEnd);

    HyprlandAPI::reloadConfig();

    return {"hyprgrass-pipewire", "Touchscreen gestures", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pDebouncer.reset();
    g_pAudioBackend.reset();
    g_pGlobalState.reset();
}
