#include "../../src/version.hpp"
#include "Debouncer.hpp"
#include "Gesture.hpp"
#include "backlight.hpp"
#include "globals.hpp"
#include "src/managers/input/trackpad/GestureTypes.hpp"

#include <cstdlib>
#include <hyprgraphics/color/Color.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <memory>
#include <stdexcept>
#include <string>

const Hyprgraphics::CColor s_pluginColor = (Hyprgraphics::CColor::SSRGB{0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f});
const std::string GESTURE_KEYWORD        = "backlight-gesture";

static bool g_unloading = false;

std::shared_ptr<BacklightBackend> g_pBackend;

static bool boolXor(bool a, bool b) {
    return a != b;
}

void onDebounceTrigger() {
    static auto const SWIPE_EDGE =
        (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprgrass-backlight:edge")
            ->getDataStaticPtr();

    if (g_pGlobalState->accumulated_delta == g_pGlobalState->last_triggered_pos) {
        return;
    }

    const char configEdge = *SWIPE_EDGE[0];

    // TODO: make configurable
    const double MAX_RANGE       = 0.7; // how much percent of the screen to swipe to get from volume 0 to 100
    const int MAX_BRIGHTNESS_PCT = 100;

    bool const vert_swipe = configEdge == 'l' || configEdge == 'r';
    const double last_triggered =
        vert_swipe ? g_pGlobalState->last_triggered_pos.y : g_pGlobalState->last_triggered_pos.x;

    const double latest = vert_swipe ? g_pGlobalState->accumulated_delta.y : g_pGlobalState->accumulated_delta.x;

    double delta      = std::abs(latest - last_triggered);
    ChangeType change = boolXor(vert_swipe, latest >= last_triggered) ? ChangeType::Increase : ChangeType::Decrease;

    double steps = MAX_BRIGHTNESS_PCT * (delta / MAX_RANGE);

    if (!(ChangeType::Decrease == change && g_pBackend->get_scaled_brightness("") - steps <= 1)) {
        g_pBackend->set_brightness("", change, steps);
    } else {
        g_pBackend->set_scaled_brightness("", 1);
    }

    g_pGlobalState->last_triggered_pos = g_pGlobalState->accumulated_delta;
}

static Hyprlang::CParseResult gestureKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    CConstVarList data(RHS);

    uint32_t fingers;
    try {
        fingers = std::stoul(std::string(data[0]));
    } catch (std::invalid_argument) {
        result.setError(
            std::format("The first argument for backlight-gesture expects a number, got: {}", data[0]).c_str()
        );
        return result;
    }

    eTrackpadGestureDirection direction = g_pTrackpadGestures->dirForString(data[1]);

    int startDataIdx    = 2;
    uint32_t modMask    = 0;
    float deltaScale    = 1.F;
    bool disableInhibit = false;

    const int prefix_size = std::size(GESTURE_KEYWORD);
    for (const auto arg : std::string(LHS).substr(prefix_size)) {
        switch (arg) {
            case 'p':
                disableInhibit = true;
                break;
            default:
                result.setError((GESTURE_KEYWORD + ": invalid flag").c_str());
                return result;
        }
    }

    while (true) {

        if (data[startDataIdx].starts_with("mod:")) {
            modMask = g_pKeybindManager->stringToModMask(std::string{data[startDataIdx].substr(4)});
            startDataIdx++;
            continue;
        } else if (data[startDataIdx].starts_with("scale:")) {
            try {
                deltaScale = std::clamp(std::stof(std::string{data[startDataIdx].substr(6)}), 0.1F, 10.F);
                startDataIdx++;
                continue;
            } catch (...) {
                result.setError(
                    std::format("Invalid delta scale: {}", std::string{data[startDataIdx].substr(6)}).c_str()
                );
                return result;
            }
        }

        break;
    }

    std::expected<void, std::string> resultFromParse;

    if (data[startDataIdx] == "dispatcher")
        resultFromParse = g_pTrackpadGestures->addGesture(
            makeUnique<BacklightGesture>(std::string{data[startDataIdx + 1]}, data.join(",", startDataIdx + 2)),
            fingers, direction, modMask, deltaScale, disableInhibit
        );

    return result;
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:hyprgrass-backlight:edge", Hyprlang::CConfigValue((Hyprlang::STRING) "r")
    );

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(
            PHANDLE, "Mismatched Hyprland version! check logs for details", CHyprColor(s_pluginColor, 1.0), 5000
        );
        Log::logger->log(Log::ERR, "[hyprgrass] version mismatch!");
        Log::logger->log(Log::ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Log::logger->log(Log::ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    HyprlandAPI::addConfigKeyword(
        PHANDLE, GESTURE_KEYWORD, gestureKeyword, Hyprlang::SHandlerOptions{.allowFlags = false}
    );

    g_pGlobalState = std::make_unique<GlobalState>();
    g_pBackend     = std::make_shared<BacklightBackend>();
    g_pDebouncer   = std::make_unique<Debouncer>(g_pCompositor->m_wlEventLoop, 16, onDebounceTrigger);

    HyprlandAPI::addNotification(
        PHANDLE, "hyprgrass-backlight is currently broken with the new hook system", CHyprColor(s_pluginColor, 1.0),
        5000
    );

    HyprlandAPI::reloadConfig();

    return {"hyprgrass-backlight", "Hyprland backlight extension", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pDebouncer.reset();
    g_pBackend.reset();
    g_pGlobalState.reset();
    g_unloading = true;
}
