#include "GestureManager.hpp"
#include "TouchVisualizer.hpp"
#include "globals.hpp"
#include "src/SharedDefs.hpp"
#include "src/managers/HookSystemManager.hpp"
#include "version.hpp"

#include <any>
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/CloseGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/DispatcherGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/FloatGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/FullscreenGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/MoveGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/ResizeGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#undef private

#include <memory>
#include <string>

const CHyprColor s_pluginColor       = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};
const CHyprColor error_color         = {204. / 255.0, 2. / 255.0, 2. / 255.0, 1.0};
const std::string KEYWORD_HG_BIND    = "hyprgrass-bind";
const std::string KEYWORD_HG_GESTURE = "hyprgrass-gesture";

inline std::unique_ptr<Visualizer> g_pVisualizer;

static bool g_unloading = false;

void hkOnTouchDown(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SDownEvent>(e);

    static auto const VISUALIZE =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:debug:visualize_touch")
            ->getDataStaticPtr();

    if (**VISUALIZE)
        g_pVisualizer->onTouchDown(ev);
    cbinfo.cancelled = g_pGestureManager->onTouchDown(ev);
}

void hkOnTouchUp(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SUpEvent>(e);

    static auto const VISUALIZE =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:debug:visualize_touch")
            ->getDataStaticPtr();

    if (**VISUALIZE)
        g_pVisualizer->onTouchUp(ev);
    cbinfo.cancelled = g_pGestureManager->onTouchUp(ev);
}

void hkOnTouchMove(void* _, SCallbackInfo& cbinfo, std::any e) {
    auto ev = std::any_cast<ITouch::SMotionEvent>(e);

    static auto const VISUALIZE =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:debug:visualize_touch")
            ->getDataStaticPtr();

    if (**VISUALIZE)
        g_pVisualizer->onTouchMotion(ev);
    cbinfo.cancelled = g_pGestureManager->onTouchMove(ev);
}

static Hyprlang::CParseResult hyprgrassGestureKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    CConstVarList data(RHS);

    auto maybePattern = parseGesturePattern(data);
    if (!maybePattern) {
        result.setError(maybePattern.error().data());
        return result;
    }
    GestureConfig pattern = maybePattern.value();

    int startDataIdx = 3;
    uint32_t modMask = 0;
    float deltaScale = 1.F;

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

    CTrackpadGestures* handler = g_pShimTrackpadGestures->get(pattern.type);

    if (data[startDataIdx] == "dispatcher")
        resultFromGesture = handler->addGesture(
            makeUnique<CDispatcherTrackpadGesture>(
                std::string{data[startDataIdx + 1]}, data.join(",", startDataIdx + 2)
            ),
            pattern.fingers, pattern.direction, modMask, deltaScale
        );
    else if (data[startDataIdx] == "workspace")
        resultFromGesture = handler->addGesture(
            makeUnique<CWorkspaceSwipeGesture>(), pattern.fingers, pattern.direction, modMask, deltaScale
        );
    else if (data[startDataIdx] == "resize")
        // this handler halves the deltaScale
        resultFromGesture = handler->addGesture(
            makeUnique<CResizeTrackpadGesture>(), pattern.fingers, pattern.direction, modMask, deltaScale * 2
        );
    else if (data[startDataIdx] == "move")
        // this handler halves the deltaScale
        resultFromGesture = handler->addGesture(
            makeUnique<CMoveTrackpadGesture>(), pattern.fingers, pattern.direction, modMask, deltaScale * 2
        );
    else if (data[startDataIdx] == "special")
        resultFromGesture = handler->addGesture(
            makeUnique<CSpecialWorkspaceGesture>(std::string{data[startDataIdx + 1]}), pattern.fingers,
            pattern.direction, modMask, deltaScale
        );
    else if (data[startDataIdx] == "close")
        resultFromGesture = handler->addGesture(
            makeUnique<CCloseTrackpadGesture>(), pattern.fingers, pattern.direction, modMask, deltaScale
        );
    else if (data[startDataIdx] == "float")
        resultFromGesture = handler->addGesture(
            makeUnique<CFloatTrackpadGesture>(std::string{data[startDataIdx + 1]}), pattern.fingers, pattern.direction,
            modMask, deltaScale
        );
    else if (data[startDataIdx] == "fullscreen")
        resultFromGesture = handler->addGesture(
            makeUnique<CFullscreenTrackpadGesture>(std::string{data[startDataIdx + 1]}), pattern.fingers,
            pattern.direction, modMask, deltaScale
        );
    else if (data[startDataIdx] == "unset")
        resultFromGesture = handler->removeGesture(pattern.fingers, pattern.direction, modMask, deltaScale);
    else {
        result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
        return result;
    }

    if (!resultFromGesture) {
        result.setError(resultFromGesture.error().c_str());
        return result;
    }

    return result;
}

static void onPreConfigReload() {
    g_pGestureManager->internalBinds.clear();
    for (auto& g : g_pShimTrackpadGestures->gestures) {
        g.clearGestures();
    }
}

void onRenderStage(eRenderStage stage) {
    static auto const VISUALIZE =
        (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:touch_gestures:debug:visualize_touch")
            ->getDataStaticPtr();

    if (stage == RENDER_LAST_MOMENT && **VISUALIZE) {
        g_pVisualizer->onRender();
    }
}

SDispatchResult listInternalBinds(std::string) {
    static const DragGestureType dragGestureTypes[3] = {
        DragGestureType::SWIPE,
        DragGestureType::LONG_PRESS,
        DragGestureType::EDGE_SWIPE,
    };
    Debug::log(LOG, "[hyprgrass] Listing internal binds:");
    for (const auto& bind : g_pGestureManager->internalBinds) {
        Debug::log(LOG, "[hyprgrass] | gesture: {}", bind->key);
        Debug::log(LOG, "[hyprgrass] |     dispatcher: {}", bind->handler);
        Debug::log(LOG, "[hyprgrass] |     arg: {}", bind->arg);
        Debug::log(LOG, "[hyprgrass] |     mouse: {}", bind->mouse);
        Debug::log(LOG, "[hyprgrass] |     locked: {}", bind->locked);
        Debug::log(LOG, "[hyprgrass] |");
    }

    for (const auto& type : dragGestureTypes) {
        auto handler = g_pShimTrackpadGestures->get(type);
        for (const auto& g : handler->m_gestures) {
            DragGestureEvent gev = {
                .time         = 0,
                .type         = type,
                .direction    = toHyprgrassDirection(g->direction),
                .finger_count = static_cast<uint32_t>(g->fingerCount),
                .edge_origin  = static_cast<uint32_t>(g->fingerCount),
            };
            Debug::log(LOG, "[hyprgrass] | gesture: {}", gev.to_string());
            Debug::log(LOG, "[hyprgrass] |     modifiers: {}", g->modMask);
            Debug::log(LOG, "[hyprgrass] |     scaling: {}", g->deltaScale);
        }
    }
    return SDispatchResult{.success = true};
}

SDispatchResult listHooks(std::string event) {
    Debug::log(LOG, "[hyprgrass] Listing hooks:");

    if (event != "") {
        const auto* vec = g_pHookSystem->getVecForEvent(event);
        Debug::log(LOG, "[hyprgrass] listeners of {}: {}", event, vec->size());
        return SDispatchResult{.success = true};
    }

    const auto* vec = g_pHookSystem->getVecForEvent("hyprgrass:edgeBegin");
    Debug::log(LOG, "[hyprgrass] | edgeBegin listeners: {}", vec->size());

    vec = g_pHookSystem->getVecForEvent("hyprgrass:edgeUpdate");
    Debug::log(LOG, "[hyprgrass] | edgeUpdate listeners: {}", vec->size());

    vec = g_pHookSystem->getVecForEvent("hyprgrass:edgeEnd");
    Debug::log(LOG, "[hyprgrass] | edgeEnd listeners: {}", vec->size());
    return SDispatchResult{.success = true};
}

Hyprlang::CParseResult hyrgrassBindKeyword(const char* K, const char* V) {
    std::string v = V;
    auto vars     = CVarList(v, 4);
    Hyprlang::CParseResult result;
    struct {
        bool mouse;
        bool locked;
    } flags = {};

    if (vars.size() < 3) {
        result.setError("must have at least 3 fields: <empty>, <gesture_event>, <dispatcher>, [args]");
        return result;
    }

    uint32_t modMask = g_pKeybindManager->stringToModMask(vars[0]);

    const int prefix_size = std::size(KEYWORD_HG_BIND);
    for (char c : std::string(K).substr(prefix_size)) {
        switch (c) {
            case 'm':
                flags.mouse = true;
                break;
            case 'l':
                flags.locked = true;
                break;
            default:
                HyprlandAPI::addNotification(
                    PHANDLE, std::string("ignoring invalid hyprgrass-bind flag: ") + c, error_color, 5000
                );
        }
    }

    const auto key            = vars[1];
    const auto dispatcher     = flags.mouse ? "mouse" : vars[2];
    const auto dispatcherArgs = flags.mouse ? vars[2] : vars[3];

    g_pGestureManager->internalBinds.emplace_back(makeShared<SKeybind>(SKeybind{
        .key     = key,
        .modmask = modMask,
        .handler = dispatcher,
        .arg     = dispatcherArgs,
        .locked  = flags.locked,
        .mouse   = flags.mouse,
    }));

    return result;
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

    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers", Hyprlang::CConfigValue((Hyprlang::INT)3)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:workspace_swipe_edge", Hyprlang::CConfigValue((Hyprlang::STRING) "d")
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:sensitivity", Hyprlang::CConfigValue((Hyprlang::FLOAT)1.0)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:long_press_delay", Hyprlang::CConfigValue((Hyprlang::INT)400)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:edge_margin", Hyprlang::CConfigValue((Hyprlang::INT)10)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:experimental:send_cancel", Hyprlang::CConfigValue((Hyprlang::INT)1)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:resize_on_border_long_press", Hyprlang::CConfigValue((Hyprlang::INT)1)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:emulate_touchpad_swipe", Hyprlang::CConfigValue((Hyprlang::INT)0)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:debug:visualize_touch", Hyprlang::CConfigValue((Hyprlang::INT)0)
    );
    HyprlandAPI::addConfigValue(
        PHANDLE, "plugin:touch_gestures:pinch_threshold", Hyprlang::CConfigValue((Hyprlang::FLOAT)0.4)
    );

    HyprlandAPI::addConfigKeyword(
        PHANDLE, KEYWORD_HG_BIND, hyrgrassBindKeyword, Hyprlang::SHandlerOptions{.allowFlags = true}
    );
    HyprlandAPI::addConfigKeyword(PHANDLE, KEYWORD_HG_GESTURE, hyprgrassGestureKeyword, Hyprlang::SHandlerOptions{});
    static auto P0 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); }
    );

    HyprlandAPI::addDispatcherV2(PHANDLE, "touchBind", [&](std::string args) {
        HyprlandAPI::addNotification(
            PHANDLE, "[hyprgrass] touchBind dispatcher deprecated, use the hyprgrass-bind keyword instead",
            CHyprColor(0.8, 0.2, 0.2, 1.0), 5000
        );
        g_pGestureManager->touchBindDispatcher(args);
        return SDispatchResult{
            .success = true,
        };
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprgrass:debug:binds", listInternalBinds);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprgrass:debug:hooks", listHooks);

    const std::string hlTargetVersion = __hyprland_api_get_hash();
    const std::string hlVersion       = __hyprland_api_get_client_hash();

    if (hlVersion != hlTargetVersion) {
        HyprlandAPI::addNotification(
            PHANDLE, "Mismatched Hyprland version! check logs for details", CHyprColor(0.8, 0.7, 0.26, 1.0), 5000
        );
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion);
    }

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchDown", hkOnTouchDown);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchUp", hkOnTouchUp);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchMove", hkOnTouchMove);
    static auto P4 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", [](void*, SCallbackInfo, std::any arg) {
        onRenderStage(std::any_cast<eRenderStage>(arg));
    });

    HyprlandAPI::reloadConfig();

    g_pGestureManager       = std::make_unique<GestureManager>();
    g_pShimTrackpadGestures = std::make_unique<ShimTrackpadGestures>();
    g_pVisualizer           = std::make_unique<Visualizer>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
    g_unloading = true;
}
