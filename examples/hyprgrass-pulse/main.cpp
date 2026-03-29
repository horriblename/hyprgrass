#include "../../src/version.hpp"
#include "Debouncer.hpp"
#include "Gesture.hpp"
#include "globals.hpp"
#include "pulse.hpp"
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
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <stdexcept>
#include <string>

const Hyprgraphics::CColor s_pluginColor = (Hyprgraphics::CColor::SSRGB{0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f});
const std::string GESTURE_KEYWORD        = "pulse-gesture";

static bool g_unloading = false;

static std::shared_ptr<AudioBackend> g_pAudioBackend;

void onDebounceTrigger() {
    static auto const PSWIPEDIST =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "gestures:workspace_swipe_distance")
            ->getDataStaticPtr();

    if (g_pGlobalState->accumulated_delta == 0.0) {
        return;
    }

    const int PA_MAX_VOLUME = 100;

    float delta       = std::abs(g_pGlobalState->accumulated_delta);
    ChangeType change = g_pGlobalState->accumulated_delta > 0.0 ? ChangeType::Increase : ChangeType::Decrease;

    double steps = PA_MAX_VOLUME * (delta / **PSWIPEDIST);

    g_pAudioBackend->changeVolume(change, steps, PA_MAX_VOLUME);

    g_pGlobalState->accumulated_delta = 0.0;
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
        result.setError(std::format("The first argument for pulse-gesture expects a number, got: {}", data[0]).c_str());
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

    std::expected<void, std::string> resultFromGesture;

    if (data[startDataIdx] == "volume")
        resultFromGesture = g_pTrackpadGestures->addGesture(
            makeUnique<PulseGesture>(), fingers, direction, modMask, deltaScale, disableInhibit
        );
    else if (data[startDataIdx] == "unset")
        resultFromGesture = g_pTrackpadGestures->removeGesture(fingers, direction, modMask, deltaScale, disableInhibit);
    else {
        result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
        return result;
    }

    return result;
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(
            PHANDLE, "Mismatched Hyprland version! check logs for details", CHyprColor(s_pluginColor, 1.0), 5000
        );
        Log::logger->log(Log::ERR, "[hyprgrass-pulse] version mismatch!");
        Log::logger->log(Log::ERR, "[hyprgrass-pulse] | hyprgrass was built against: {}", hlTargetVersion);
        Log::logger->log(Log::ERR, "[hyprgrass-pulse] | actual hyprland version: {}", hlVersion.hash);
    }

    g_pGlobalState  = makeUnique<GlobalState>();
    g_pAudioBackend = AudioBackend::getInstance();
    g_pDebouncer    = makeUnique<Debouncer>(g_pCompositor->m_wlEventLoop, 16, onDebounceTrigger);

    g_pGlobalState->accumulated_delta = 1.0;

    HyprlandAPI::addConfigKeyword(
        PHANDLE, GESTURE_KEYWORD, gestureKeyword, Hyprlang::SHandlerOptions{.allowFlags = true}
    );

    HyprlandAPI::reloadConfig();

    return {"hyprgrass-pulse", "Hyprland pulseaudio extension", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pDebouncer.reset();
    g_pAudioBackend.reset();
    g_pGlobalState.reset();
    g_unloading = true;
}
