#include "../../src/version.hpp"
#include "Debouncer.hpp"
#include "globals.hpp"
#include "pulse.hpp"

#include <any>
#include <cstdlib>
#include <hyprgraphics/color/Color.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <memory>
#include <string>
#include <utility>

const Hyprgraphics::CColor s_pluginColor = (Hyprgraphics::CColor::SSRGB{0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f});

std::shared_ptr<AudioBackend> g_pAudioBackend;
std::unique_ptr<Debouncer> g_pDebouncer;
struct GlobalState {
    Vector2D last_triggered_pos;
    Vector2D latest_pos;
};
std::unique_ptr<GlobalState> g_pGlobalState;

constexpr int ORIGIN_CHAR_INDEX    = 5;
constexpr int DIRECTION_CHAR_INDEX = 7;

void onEdgeBegin(void*, Event::SCallbackInfo& cbinfo, std::any args) {
    auto ev = std::any_cast<std::pair<std::string, Vector2D>>(args);

    static auto const SWIPE_EDGE =
        (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprgrass-pulse:edge")
            ->getDataStaticPtr();

    // we only accept "edge:a:b"
    // where a is a single char of the origin side
    // and b is a single char of the swipe direction
    if (ev.first.size() != 8) {
        return;
    }

    const char configEdge = *SWIPE_EDGE[0];

    char origin    = ev.first[ORIGIN_CHAR_INDEX];
    char direction = ev.first[DIRECTION_CHAR_INDEX];
    if (origin != configEdge) {
        return;
    }

    switch (configEdge) {
        case 'l': /*fallthrough*/
        case 'r':
            if (direction != 'u' && direction != 'd') {
                return;
            }
            break;

        case 'u': /*fallthrough*/
        case 'd':
            if (direction != 'l' && direction != 'r') {
                return;
            }
            break;

        default:
            return;
    }

    g_pGlobalState->latest_pos         = ev.second;
    g_pGlobalState->last_triggered_pos = ev.second;

    cbinfo.cancelled = true;
}

void onEdgeUpdate(void*, Event::SCallbackInfo& cbinfo, std::any args) {
    auto pos = std::any_cast<Vector2D>(args);

    g_pGlobalState->latest_pos = pos;
    g_pDebouncer->start();
}

void onEdgeEnd(void*, Event::SCallbackInfo& cbinfo, std::any args) {
    g_pDebouncer->disarm();
}

bool boolXor(bool a, bool b) {
    return a != b;
}

void onDebounceTrigger() {
    static auto const SWIPE_EDGE =
        (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprgrass-pulse:edge")
            ->getDataStaticPtr();

    if (g_pGlobalState->latest_pos == g_pGlobalState->last_triggered_pos) {
        return;
    }

    const char configEdge = *SWIPE_EDGE[0];

    // TODO: make configurable
    const double MAX_RANGE  = 0.7; // how much percent of the screen to swipe to get from volume 0 to 100
    const int PA_MAX_VOLUME = 100;

    bool const vert_swipe = configEdge == 'l' || configEdge == 'r';
    const double last_triggered =
        vert_swipe ? g_pGlobalState->last_triggered_pos.y : g_pGlobalState->last_triggered_pos.x;

    const double latest = vert_swipe ? g_pGlobalState->latest_pos.y : g_pGlobalState->latest_pos.x;

    double delta      = std::abs(latest - last_triggered);
    ChangeType change = boolXor(vert_swipe, latest >= last_triggered) ? ChangeType::Increase : ChangeType::Decrease;

    double steps = PA_MAX_VOLUME * (delta / MAX_RANGE);

    g_pAudioBackend->changeVolume(change, steps, PA_MAX_VOLUME);

    g_pGlobalState->last_triggered_pos = g_pGlobalState->latest_pos;
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprgrass-pulse:edge", Hyprlang::CConfigValue((Hyprlang::STRING) "l"));

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CHyprColor(s_pluginColor, 1.0), 5000);
        Log::logger->log(Log::ERR, "[hyprgrass] version mismatch!");
        Log::logger->log(Log::ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Log::logger->log(Log::ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    g_pGlobalState  = std::make_unique<GlobalState>();
    g_pAudioBackend = AudioBackend::getInstance();
    g_pDebouncer    = std::make_unique<Debouncer>(g_pCompositor->m_wlEventLoop, 16, onDebounceTrigger);

    // FIXME: figure out custom events for new hook system
    /*
    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeBegin", onEdgeBegin);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeUpdate", onEdgeUpdate);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "hyprgrass:edgeEnd", onEdgeEnd);

    if (!P1 || !P2 || !P3) {
        Log::logger->log(Log::DEBUG, "[hyprgrass pulse] something went wrong: could not register hooks");
    }
    */
    HyprlandAPI::addNotification(PHANDLE, "hyprgrass-pulse no longer works with the new hook system", CHyprColor(s_pluginColor, 1.0), 5000);

    HyprlandAPI::reloadConfig();

    return {"hyprgrass-pulse", "Hyprgrass pulseaudio extension", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pDebouncer.reset();
    g_pAudioBackend.reset();
    g_pGlobalState.reset();
}
