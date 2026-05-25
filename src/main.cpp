#include "EmulateTouchpadGesture.hpp"
#include "GestureManager.hpp"
#include "TouchVisualizer.hpp"
#include "gestures/DragGesture.hpp"
#include "globals.hpp"
#include "version.hpp"

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/CloseGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/CursorZoomGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/DispatcherGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/FloatGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/FullscreenGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/LuaFunctionGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/MoveGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/ResizeGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/ScrollMoveGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp>
#include <hyprland/src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>

#include <hyprlang.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#undef private

#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

const CHyprColor s_pluginColor       = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};
const CHyprColor error_color         = {204. / 255.0, 2. / 255.0, 2. / 255.0, 1.0};
const std::string KEYWORD_HG_BIND    = "hyprgrass-bind";
const std::string KEYWORD_HG_GESTURE = "hyprgrass-gesture";

inline std::unique_ptr<Visualizer> g_pVisualizer;

static bool g_unloading = false;

void hkOnTouchDown(ITouch::SDownEvent ev, Event::SCallbackInfo& cbinfo) {
    cbinfo.cancelled = g_pGestureManager->onTouchDown(ev);
}

void hkOnTouchUp(ITouch::SUpEvent ev, Event::SCallbackInfo& cbinfo) {
    cbinfo.cancelled = g_pGestureManager->onTouchUp(ev);
}

void hkOnTouchMove(ITouch::SMotionEvent ev, Event::SCallbackInfo& cbinfo) {
    cbinfo.cancelled = g_pGestureManager->onTouchMove(ev);
}

static Hyprlang::CParseResult hyprgrassGestureKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    Hyprutils::String::CConstVarList data(RHS);

    auto maybePattern = parseGesturePattern(data);
    if (!maybePattern) {
        result.setError(maybePattern.error().data());
        return result;
    }
    GestureConfig pattern = maybePattern.value();

    int startDataIdx    = 3;
    uint32_t modMask    = 0;
    float deltaScale    = 1.F;
    bool disableInhibit = false;

    const int prefix_size = std::size(KEYWORD_HG_GESTURE);
    for (const auto arg : std::string(LHS).substr(prefix_size)) {
        switch (arg) {
            case 'p':
                disableInhibit = true;
                break;
            default:
                result.setError("hyprgrass-gesture: invalid flag");
                return result;
        }
    }

    while (true) {

        if (data[startDataIdx].starts_with("mod:")) {
            modMask = g_pKeybindManager->stringToModMask(std::string(data[startDataIdx].substr(4)));
            startDataIdx++;
            continue;
        } else if (data[startDataIdx].starts_with("scale:")) {
            try {
                deltaScale = std::clamp(std::stof(std::string(data[startDataIdx].substr(6))), 0.1F, 10.F);
                startDataIdx++;
                continue;
            } catch (...) {
                result.setError(
                    std::format("Invalid delta scale: {}", std::string(data[startDataIdx].substr(6))).c_str()
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
                std::string(data[startDataIdx + 1]), data.join(",", startDataIdx + 2)
            ),
            pattern.fingers(), pattern.direction, modMask, deltaScale, disableInhibit
        );
    else if (data[startDataIdx] == "workspace")
        resultFromGesture = handler->addGesture(
            makeUnique<CWorkspaceSwipeGesture>(), pattern.fingers(), pattern.direction, modMask, deltaScale,
            disableInhibit
        );
    else if (data[startDataIdx] == "resize")
        // this handler halves the deltaScale
        resultFromGesture = handler->addGesture(
            makeUnique<CResizeTrackpadGesture>(), pattern.fingers(), pattern.direction, modMask, deltaScale * 2,
            disableInhibit
        );
    else if (data[startDataIdx] == "move")
        // this handler halves the deltaScale
        resultFromGesture = handler->addGesture(
            makeUnique<CMoveTrackpadGesture>(), pattern.fingers(), pattern.direction, modMask, deltaScale * 2,
            disableInhibit
        );
    else if (data[startDataIdx] == "special")
        resultFromGesture = handler->addGesture(
            makeUnique<CSpecialWorkspaceGesture>(std::string(data[startDataIdx + 1])), pattern.fingers(),
            pattern.direction, modMask, deltaScale, disableInhibit
        );
    else if (data[startDataIdx] == "close")
        resultFromGesture = handler->addGesture(
            makeUnique<CCloseTrackpadGesture>(), pattern.fingers(), pattern.direction, modMask, deltaScale,
            disableInhibit
        );
    else if (data[startDataIdx] == "float")
        resultFromGesture = handler->addGesture(
            makeUnique<CFloatTrackpadGesture>(std::string(data[startDataIdx + 1])), pattern.fingers(),
            pattern.direction, modMask, deltaScale, disableInhibit
        );
    else if (data[startDataIdx] == "fullscreen")
        resultFromGesture = handler->addGesture(
            makeUnique<CFullscreenTrackpadGesture>(std::string(data[startDataIdx + 1])), pattern.fingers(),
            pattern.direction, modMask, deltaScale, disableInhibit
        );
    else if (data[startDataIdx] == "unset")
        resultFromGesture =
            handler->removeGesture(pattern.fingers(), pattern.direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "emulate_touchpad") {
        const auto fingersStr = data[startDataIdx + 1];
        uint32_t fingers      = 0;

        try {
            fingers = std::stoul(std::string(fingersStr));
        } catch (std::invalid_argument&) {
            result.setError(std::format("Argument for emulate_touchpad expects a number, got: {}", fingersStr).c_str());
            return result;
        }

        eTrackpadGestureDirection dir = g_pTrackpadGestures->dirForString(data[startDataIdx + 2]);
        if (ShimTrackpadGestures::isPinch(pattern.direction) != ShimTrackpadGestures::isPinch(dir)) {
            if (ShimTrackpadGestures::isPinch(dir)) {
                result.setError("emulate_touchpad: pinch gestures need to be bound to pinch touch direction");
            } else {
                result.setError(
                    "emulate_touchpad: non-pinch gestures need to be bound to bind to a non-pinch touch direction"
                );
            }
            return result;
        }

        resultFromGesture = std::expected(handler->addGesture(
            makeUnique<EmulateTouchpadGesture>(fingers, dir), pattern.fingers(), pattern.direction, modMask, deltaScale,
            disableInhibit
        ));
    } else {
        result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
        return result;
    }

    if (!resultFromGesture) {
        result.setError(resultFromGesture.error().c_str());
        return result;
    }

    return result;
}

static bool luaTableGetBool(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    const bool v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

static int luaTableGetInt(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    const int v = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}

// Note: we don't own the returned string
static const char* luaTableGetString(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    const char* v = lua_tostring(L, -1);
    lua_pop(L, 1);
    return v;
}

static std::optional<float> luaTableMaybeGetFloat(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    if (lua_isnil(L, -1))
        return std::nullopt;
    float v = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

// Note: we don't own the returned string
static std::optional<const char*> luaTableMaybeGetString(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    if (lua_isnil(L, -1))
        return std::nullopt;
    const char* v = lua_tostring(L, -1);
    lua_pop(L, 1);
    return v;
}

int newBind(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(
            L, "bind: expected a table { mod, gesture, dispatcher, args }"
        );

    SKeybind bind{};

    // Parse the table structure
    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "mod");
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1))
                return Config::Lua::Bindings::Internal::configError(L, "bind: mod must be a string");

            const char* modStr = lua_tostring(L, -1);
            bind.modmask       = g_pKeybindManager->stringToModMask(modStr);
        }
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "gesture");
        if (!lua_isstring(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "bind: gesture must be a string");

        bind.key = lua_tostring(L, -1);
        // TODO: idk what this is
        bind.displayKey = bind.key;
    }

    lua_getfield(L, 1, "action");
    if (!Config::Lua::Bindings::Internal::pushDispatcherFunction(L, 2)) {
        return Config::Lua::Bindings::Internal::configError(
            L, "hl.plugin.hyprgrass.bind: action must be a dispatcher (e.g. hl.dsp.window.close()) or a lua function"
        );
    }

    int ref      = luaL_ref(L, LUA_REGISTRYINDEX);
    bind.handler = "__lua";
    bind.arg     = std::to_string(ref);

    bind.mouse  = luaTableGetBool(L, 1, "mouse");
    bind.locked = luaTableGetBool(L, 1, "locked");

    g_pGestureManager->internalBinds.emplace_back(makeShared<SKeybind>(bind));

    return 0;
}

std::expected<GestureConfig, std::string> gestureConfigFromTable(lua_State* L, int index) {
    std::string kind = luaTableGetString(L, index, "kind");
    DragGestureType type;
    size_t fingersOrOrigin              = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    if (kind == "swipe") {
        type = DragGestureType::SWIPE;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin <= 0)
            return std::unexpected("kind=swipe: fingers must be a positive integer");

        const char* dirStr = luaTableGetString(L, index, "direction");
        if (!dirStr)
            return std::unexpected("kind=swipe: direction must be a valid direction string");
        direction = g_pTrackpadGestures->dirForString(dirStr);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE)
            return std::unexpected(std::format("invalid direction for a swipe gesture: {}", dirStr));
    } else if (kind == "edge") {
        type = DragGestureType::EDGE_SWIPE;

        const char* originStr = luaTableGetString(L, index, "origin");
        if (!originStr)
            return std::unexpected("kind=edge: origin must be a valid direction string");
        auto origin = g_pTrackpadGestures->dirForString(originStr);
        if (!ShimTrackpadGestures::isSingleDirection(origin))
            return std::unexpected(
                std::format("invalid ORIGIN for an edge gesture, expected a single direction, got {}", originStr)
            );
        fingersOrOrigin = toHyprgrassDirection(origin);

        const char* dirStr = luaTableGetString(L, index, "direction");
        if (!dirStr)
            return std::unexpected("kind=edge: direction must be a valid direction string");
        direction = g_pTrackpadGestures->dirForString(dirStr);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE)
            return std::unexpected(std::format("invalid direction for an edge gesture: {}", dirStr));
    } else if (kind == "longpress") {
        type = DragGestureType::LONG_PRESS;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin == 0)
            return std::unexpected("longpress: fingers must be a positive integer");
    } else {
        return std::unexpected(std::format("invalid gesture kind: {}", kind));
    }

    return GestureConfig{
        .type            = type,
        .direction       = direction,
        .fingersOrOrigin = fingersOrOrigin,
    };
}

int newGesture(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(L, "hl.plugin.hyprgrass.gesture: argument must be a table");

    GestureConfig gesture;
    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });
        lua_getfield(L, 1, "gesture");

        auto maybeGesture = gestureConfigFromTable(L, 2);
        if (!maybeGesture)
            return Config::Lua::Bindings::Internal::configError(L, maybeGesture.error());

        gesture = maybeGesture.value();
    }

    auto maybeMod    = luaTableMaybeGetString(L, 1, "mod");
    uint32_t modMask = maybeMod ? g_pKeybindManager->stringToModMask(maybeMod.value()) : 0;

    auto maybeScale  = luaTableMaybeGetFloat(L, 1, "scale");
    float deltaScale = maybeScale ? maybeScale.value() : 1.f;
    // TODO: standardize parsing
    if (deltaScale < 0.1f) {
        return Config::Lua::Bindings::Internal::configError(
            L, "hl.plugin.hyprgrass.gesture: field \"scale\" must be at least 0.1"
        );
    }

    int functionRef         = LUA_NOREF;
    std::string_view action = "";
    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });
        lua_getfield(L, 1, "action");

        if (lua_isstring(L, -1)) {
            action = lua_tostring(L, -1);
        } else if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, -1);
            functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
            Config::Lua::mgr()->registerLuaRef(functionRef);
        } else if (Config::Lua::Bindings::Internal::pushDispatcherFunction(L, -1)) {
            functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
            lua_pop(L, 1);
        } else {
            return Config::Lua::Bindings::Internal::configError(
                L, "hl.plugin.hyprgrass.gesture: action must be a string (e.g. \"workspace\"), lua function, or "
                   "dispatcher (e.g. hl.dsp.focus(...))"
            );
        }
    }

    CTrackpadGestures* handler = g_pShimTrackpadGestures->get(gesture.type);
    std::expected<void, std::string> result;

    auto workspaceName = luaTableMaybeGetString(L, 1, "workspace_name").value_or("");
    auto zoomLevel     = luaTableMaybeGetString(L, 1, "zoom_level").value_or("");
    auto mode          = luaTableMaybeGetString(L, 1, "mode").value_or("");

    // TODO: impl
    const bool disableInhibit = false;

    if (functionRef != LUA_NOREF) {
        result = handler->addGesture(
            makeUnique<CLuaFunctionGesture>(functionRef), gesture.fingers(), gesture.direction, modMask, deltaScale,
            disableInhibit
        );
    } else {
        if (action == "workspace")
            result = handler->addGesture(
                makeUnique<CWorkspaceSwipeGesture>(), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        else if (action == "resize") {
            result = handler->addGesture(
                makeUnique<CResizeTrackpadGesture>(), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "move") {
            result = handler->addGesture(
                makeUnique<CMoveTrackpadGesture>(), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "special") {
            result = handler->addGesture(
                makeUnique<CSpecialWorkspaceGesture>(workspaceName), gesture.fingers(), gesture.direction, modMask,
                deltaScale, disableInhibit
            );
        } else if (action == "close") {
            result = handler->addGesture(
                makeUnique<CCloseTrackpadGesture>(), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "float") {
            result = handler->addGesture(
                makeUnique<CFloatTrackpadGesture>(mode), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "fullscreen") {
            result = handler->addGesture(
                makeUnique<CFullscreenTrackpadGesture>(mode), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "cursor_zoom" || action == "cursorZoom") {
            result = handler->addGesture(
                makeUnique<CCursorZoomTrackpadGesture>(zoomLevel, mode), gesture.fingers(), gesture.direction, modMask,
                deltaScale, disableInhibit
            );
        } else if (action == "scroll_move") {
            result = handler->addGesture(
                makeUnique<CScrollMoveTrackpadGesture>(), gesture.fingers(), gesture.direction, modMask, deltaScale,
                disableInhibit
            );
        } else if (action == "emulate_touchpad") {
            result = std::expected(handler->addGesture(
                makeUnique<EmulateTouchpadGesture>(gesture.fingers(), gesture.direction), gesture.fingers(),
                gesture.direction, modMask, deltaScale, disableInhibit
            ));
        } else if (action == "unset") {
            result = handler->removeGesture(gesture.fingers(), gesture.direction, modMask, deltaScale, disableInhibit);
        } else
            return Config::Lua::Bindings::Internal::configError(
                L, std::format("hl.gesture: unknown action \"{}\"", action)
            );
    }

    if (!result) {
        return Config::Lua::Bindings::Internal::configError(L, result.error());
    }
    return 0;
}

static void onPreConfigReload() {
    if (g_pGestureManager)
        g_pGestureManager->internalBinds.clear();

    if (g_pShimTrackpadGestures) {
        for (auto& g : g_pShimTrackpadGestures->gestures) {
            g.clearGestures();
        }
    }
}

SDispatchResult listInternalBinds(std::string) {
    static const DragGestureType dragGestureTypes[3] = {
        DragGestureType::SWIPE,
        DragGestureType::LONG_PRESS,
        DragGestureType::EDGE_SWIPE,
    };
    Log::logger->log(Log::DEBUG, "[hyprgrass] Listing internal binds:");
    for (const auto& bind : g_pGestureManager->internalBinds) {
        Log::logger->log(Log::DEBUG, "[hyprgrass] | gesture: {}", bind->key);
        Log::logger->log(Log::DEBUG, "[hyprgrass] |     dispatcher: {}", bind->handler);
        Log::logger->log(Log::DEBUG, "[hyprgrass] |     arg: {}", bind->arg);
        Log::logger->log(Log::DEBUG, "[hyprgrass] |     mouse: {}", bind->mouse);
        Log::logger->log(Log::DEBUG, "[hyprgrass] |     locked: {}", bind->locked);
        Log::logger->log(Log::DEBUG, "[hyprgrass] |");
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
            Log::logger->log(Log::DEBUG, "[hyprgrass] | gesture: {}", gev.to_string());
            Log::logger->log(Log::DEBUG, "[hyprgrass] |     modifiers: {}", g->modMask);
            Log::logger->log(Log::DEBUG, "[hyprgrass] |     scaling: {}", g->deltaScale);
        }
    }
    return SDispatchResult{.success = true};
}

Hyprlang::CParseResult hyrgrassBindKeyword(const char* K, const char* V) {
    std::string v = V;
    auto vars     = Hyprutils::String::CVarList(v, 4);
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

    if (Config::mgr()->type() == Config::CONFIG_LEGACY) {
        g_config = makeUnique<Cfg>("touch_gestures");
        HyprlandAPI::addConfigKeyword(
            PHANDLE, KEYWORD_HG_BIND, hyrgrassBindKeyword, Hyprlang::SHandlerOptions{.allowFlags = true}
        );
        HyprlandAPI::addConfigKeyword(
            PHANDLE, KEYWORD_HG_GESTURE, hyprgrassGestureKeyword, Hyprlang::SHandlerOptions{true}
        );
    } else {
        g_config = makeUnique<Cfg>("hyprgrass");
        HyprlandAPI::addLuaFunction(PHANDLE, "hyprgrass", "bind", newBind);
        HyprlandAPI::addLuaFunction(PHANDLE, "hyprgrass", "gesture", newGesture);
        HyprlandAPI::addLuaFunction(PHANDLE, "hyprgrass", "debug_binds", [](lua_State*) {
            listInternalBinds("");
            return 0;
        });
        HyprlandAPI::addLuaFunction(PHANDLE, "hyprgrass", "debug_gestures", [](lua_State*) {
            g_pShimTrackpadGestures->listGestures();
            return 0;
        });
    }

    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->workspaceSwipeFingers);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->longPressDelay);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->edgeMargin);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->workspaceSwipeEdge);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->sensitivity);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->sendCancel);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->resizeOnBorder);

    static auto P0 = Event::bus()->m_events.config.preReload.listen([&] { onPreConfigReload(); });

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

    const std::string hlTargetVersion = __hyprland_api_get_hash();
    const std::string hlVersion       = __hyprland_api_get_client_hash();

    if (hlVersion != hlTargetVersion) {
        HyprlandAPI::addNotification(
            PHANDLE, "Mismatched Hyprland version! check logs for details", CHyprColor(0.8, 0.7, 0.26, 1.0), 5000
        );
        Log::logger->log(Log::ERR, "[hyprgrass] version mismatch!");
        Log::logger->log(Log::ERR, "[hyprgrass] | hyprgrass was built against: {}", hlTargetVersion);
        Log::logger->log(Log::ERR, "[hyprgrass] | actual hyprland version: {}", hlVersion);
    }

    static auto P1 = Event::bus()->m_events.input.touch.down.listen(hkOnTouchDown);
    static auto P2 = Event::bus()->m_events.input.touch.up.listen(hkOnTouchUp);
    static auto P3 = Event::bus()->m_events.input.touch.motion.listen(hkOnTouchMove);

    HyprlandAPI::reloadConfig();

    g_pGestureManager       = std::make_unique<GestureManager>();
    g_pShimTrackpadGestures = std::make_unique<ShimTrackpadGestures>();

    return {"hyprgrass", "Touchscreen gestures", "horriblename", HYPRGRASS_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_unloading = true;
}
