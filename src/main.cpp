#include "GestureManager.hpp"
#include "TouchVisualizer.hpp"
#include "globals.hpp"
#include "src/SharedDefs.hpp"
#include "src/managers/HookSystemManager.hpp"
#include "version.hpp"

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <memory>
#include <string>

const CHyprColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

inline std::unique_ptr<Visualizer> g_pVisualizer;

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

static void onPreConfigReload() {
    g_pGestureManager->internalBinds.clear();
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
    Debug::log(LOG, "[hyprgrass] Listing internal binds:");
    for (const auto& bind : g_pGestureManager->internalBinds) {
        Debug::log(LOG, "[hyprgrass] | gesture: {}", bind->key);
        Debug::log(LOG, "[hyprgrass] |     dispatcher: {}", bind->handler);
        Debug::log(LOG, "[hyprgrass] |     arg: {}", bind->arg);
        Debug::log(LOG, "[hyprgrass] |     mouse: {}", bind->mouse);
        Debug::log(LOG, "[hyprgrass] |");
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

Hyprlang::CParseResult onNewBind(const char* K, const char* V) {
    std::string v = V;
    auto vars     = CVarList(v, 4);
    Hyprlang::CParseResult result;

    if (vars.size() < 3) {
        result.setError("must have at least 3 fields: <empty>, <gesture_event>, <dispatcher>, [args]");
        return result;
    }

    if (!vars[0].empty()) {
        result.setError("MODIFIER keys not currently supported");
        return result;
    }

    const auto mouse          = std::string("hyprgrass-bindm") == K;
    const auto key            = vars[1];
    const auto dispatcher     = mouse ? "mouse" : vars[2];
    const auto dispatcherArgs = mouse ? vars[2] : vars[3];

    g_pGestureManager->internalBinds.emplace_back(makeShared<SKeybind>(SKeybind{
        .key     = key,
        .handler = dispatcher,
        .arg     = dispatcherArgs,
        .mouse   = mouse,
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

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_fingers",
                                Hyprlang::CConfigValue((Hyprlang::INT)3));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:workspace_swipe_edge",
                                Hyprlang::CConfigValue((Hyprlang::STRING) "d"));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:sensitivity",
                                Hyprlang::CConfigValue((Hyprlang::FLOAT)1.0));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:long_press_delay",
                                Hyprlang::CConfigValue((Hyprlang::INT)400));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:edge_margin",
                                Hyprlang::CConfigValue((Hyprlang::INT)10));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:experimental:send_cancel",
                                Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:resize_on_border_long_press",
                                Hyprlang::CConfigValue((Hyprlang::INT)1));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:emulate_touchpad_swipe",
                                Hyprlang::CConfigValue((Hyprlang::INT)0));
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:touch_gestures:debug:visualize_touch",
                                Hyprlang::CConfigValue((Hyprlang::INT)0));

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprgrass-bind", onNewBind, Hyprlang::SHandlerOptions{});
    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprgrass-bindm", onNewBind, Hyprlang::SHandlerOptions{});
    static auto P0 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    HyprlandAPI::addDispatcherV2(PHANDLE, "touchBind", [&](std::string args) {
        HyprlandAPI::addNotification(
            PHANDLE, "[hyprgrass] touchBind dispatcher deprecated, use the hyprgrass-bind keyword instead",
            CHyprColor(0.8, 0.2, 0.2, 1.0), 5000);
        g_pGestureManager->touchBindDispatcher(args);
        return SDispatchResult{
            .success = true,
        };
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprgrass:debug:binds", listInternalBinds);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprgrass:debug:hooks", listHooks);

    const auto hlTargetVersion = GIT_COMMIT_HASH;
    const auto hlVersion       = HyprlandAPI::getHyprlandVersion(PHANDLE);

    if (hlVersion.hash != hlTargetVersion) {
        HyprlandAPI::addNotification(PHANDLE, "Mismatched Hyprland version! check logs for details",
                                     CHyprColor(0.8, 0.7, 0.26, 1.0), 5000);
        Debug::log(ERR, "[hyprgrass] version mismatch!");
        Debug::log(ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Debug::log(ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion.hash);
    }

    static auto P1 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchDown", hkOnTouchDown);
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchUp", hkOnTouchUp);
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "touchMove", hkOnTouchMove);
    static auto P4 = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render", [](void*, SCallbackInfo, std::any arg) { onRenderStage(std::any_cast<eRenderStage>(arg)); });

    HyprlandAPI::reloadConfig();

    g_pGestureManager = std::make_unique<GestureManager>();
    g_pVisualizer     = std::make_unique<Visualizer>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // idk if I should do this, but just in case
    g_pGestureManager.reset();
}
