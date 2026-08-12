#include "EmulateTouchpadGesture.hpp"
#include "GestureManager.hpp"
#include "gestures/CompletedGesture.hpp"
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

const CHyprColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};
const CHyprColor error_color   = {204. / 255.0, 2. / 255.0, 2. / 255.0, 1.0};

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

static std::expected<std::optional<float>, std::string>
luaTableMaybeGetFloat(lua_State* L, int idx, std::string_view key) {
    int valid;

    lua_getfield(L, idx, key.data());
    Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

    if (lua_isnil(L, -1))
        return std::nullopt;

    float v = lua_tonumberx(L, -1, &valid);

    if (!valid) {
        return std::unexpected{
            std::format("in field \"{}\": expected an optional number, got \"{}\"", key, lua_tostring(L, -1))
        };
    }

    return v;
}

static std::expected<GesturePattern, std::string> gesturePatternFromTable(lua_State* L, int index, bool hgGesture);

static std::expected<std::optional<std::string_view>, std::string>
luaTableMaybeGetString(lua_State* L, int idx, std::string_view key) {
    lua_getfield(L, idx, key.data());
    Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

    if (lua_isnil(L, -1))
        return std::nullopt;

    const char* v = lua_tostring(L, -1);

    if (v == NULL) {
        return std::unexpected{std::format("in field \"{}\": expected a string", key)};
    }
    return v;
}

static std::expected<std::string_view, std::string> luaTableGetString(lua_State* L, int idx, std::string_view key) {
    auto v = luaTableMaybeGetString(L, idx, key);
    if (!v)
        return std::unexpected{v.error()};
    if (v.value() == std::nullopt) {
        return std::unexpected{std::format("missing field \"{}\", expected a string", key)};
    }
    return v.value().value();
}

int newBind(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(
            L, "hyprgrass.bind: expected a table { mod, pattern, dispatcher, args }"
        );

    SKeybind bind{};

    // Parse the table structure
    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "mod");
        if (!lua_isnil(L, -1)) {
            if (!lua_isstring(L, -1))
                return Config::Lua::Bindings::Internal::configError(L, "hyprgrass.bind: mod must be a string");

            const char* modStr = lua_tostring(L, -1);
            bind.modmask       = g_pKeybindManager->stringToModMask(modStr);
        }
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "pattern");

        if (lua_isstring(L, 2)) {
            auto maybeGesture = parseGesturePattern(lua_tostring(L, 2));
            if (!maybeGesture) {
                return Config::Lua::Bindings::Internal::configError(
                    L, std::format("hyprgrass.bind: in field \"pattern\": {}", maybeGesture.error())
                );
            }
            bind.key = maybeGesture.value().to_string();
        } else {
            auto maybeGesture = gesturePatternFromTable(L, 2, false);
            if (!maybeGesture) {
                return Config::Lua::Bindings::Internal::configError(
                    L, std::format("hyprgrass.bind: in field \"pattern\": {}", maybeGesture.error())
                );
            }
            bind.key = maybeGesture.value().to_string();
        }

        // TODO: idk what this is
        bind.displayKey = bind.key;
    }

    lua_getfield(L, 1, "action");
    if (!Config::Lua::Bindings::Internal::pushDispatcherFunction(L, 2)) {
        return Config::Lua::Bindings::Internal::configError(
            L, "hyprgrass.bind: action must be a dispatcher (e.g. hl.dsp.window.close()) or a lua function"
        );
    }

    int ref      = luaL_ref(L, LUA_REGISTRYINDEX);
    bind.handler = "__lua";
    bind.arg     = std::to_string(ref);

    bind.mouse        = luaTableGetBool(L, 1, "mouse");
    bind.locked       = luaTableGetBool(L, 1, "locked");
    bind.nonConsuming = luaTableGetBool(L, 1, "non_consuming");

    g_pGestureManager->internalBinds.emplace_back(makeShared<SKeybind>(bind));

    return 0;
}

// If hgGesture is true, allows multi-directional direction where applicable, and allows
// a direction for longpress
std::expected<GesturePattern, std::string> gesturePatternFromTable(lua_State* L, int index, bool hgGesture) {
    // normalize index to positive value
    index = index > 0 ? index : lua_gettop(L) + index + 1;

    if (!lua_istable(L, index)) {
        return std::unexpected{std::format(
            "expected a table {{kind = \"swipe|edge|longpress|pinch|tap\", ...}}\n\tgot \"{}\"", lua_tostring(L, index)
        )};
    }

    auto maybeKindResult = luaTableMaybeGetString(L, index, "kind");
    if (!maybeKindResult) {
        return std::unexpected{
            std::format("invalid type in field \"kind\", expected \"swipe|edge|longpress|pinch|tap\"")
        };
    } else if (!maybeKindResult.value().has_value()) {
        return std::unexpected{std::format("missing field \"kind\", expected \"swipe|edge|longpress|pinch|tap\"")};
    }
    std::string kind{maybeKindResult.value().value()};
    GestureType type;
    size_t fingersOrOrigin              = 0;
    eTrackpadGestureDirection direction = TRACKPAD_GESTURE_DIR_NONE;

    if (kind == "swipe") {
        type = GestureType::SWIPE;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin <= 0)
            return std::unexpected("kind=swipe: fingers must be a positive integer");

        const auto dirStrResult = luaTableGetString(L, index, "direction");
        if (!dirStrResult)
            return std::unexpected{"kind=swipe: field \"direction\" must be a valid direction string"};
        const std::string_view dirStr = dirStrResult.value();
        direction                     = g_pTrackpadGestures->dirForString(dirStr);
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE)
            return std::unexpected(std::format("kind=swipe: invalid direction {}", dirStr));

        if (!hgGesture && !ShimTrackpadGestures::isSingleDirection(direction)) {
            return std::unexpected(
                std::format("direction must be left/right/up/down for hyprgrass.bind, got \"{}\"", dirStr)
            );
        }
    } else if (kind == "edge") {
        type = GestureType::EDGE_SWIPE;

        const auto originStrResult = luaTableGetString(L, index, "origin");
        if (!originStrResult)
            return std::unexpected("kind=edge: field \"origin\" must be a valid direction string");
        auto origin = g_pTrackpadGestures->dirForString(originStrResult.value());
        if (!ShimTrackpadGestures::isSingleDirection(origin))
            return std::unexpected(
                std::format(
                    "invalid origin for an edge gesture, expected a single direction, got {}", originStrResult.value()
                )
            );

        // default to 1 when unspecified
        size_t edgeFingers = luaTableGetInt(L, index, "fingers");
        if (edgeFingers == 0)
            edgeFingers = 1;
        else if (edgeFingers > FINGERS_MASK)
            return std::unexpected("kind=edge: fingers is too large");

        fingersOrOrigin = (static_cast<size_t>(toHyprgrassDirection(origin)) << MOD_MASK_SHIFT) | edgeFingers;

        const auto dirStrResult = luaTableGetString(L, index, "direction");
        if (!dirStrResult)
            return std::unexpected("kind=edge: direction must be a valid direction string");
        std::string_view dirStr = dirStrResult.value();
        direction               = g_pTrackpadGestures->dirForString(dirStrResult.value());
        if (ShimTrackpadGestures::isPinch(direction) || direction == TRACKPAD_GESTURE_DIR_NONE)
            return std::unexpected(
                std::format("invalid direction for an edge gesture: \"{}\", expected left/right/up/down", dirStr)
            );

        if (!hgGesture && !ShimTrackpadGestures::isSingleDirection(direction)) {
            return std::unexpected(
                std::format("direction must be left/right/up/down for hyprgrass.bind, got \"{}\"", dirStr)
            );
        }
    } else if (kind == "longpress") {
        type = GestureType::LONG_PRESS;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin == 0)
            return std::unexpected("kind=longpress: fingers must be a positive integer");

        if (hgGesture) {
            auto dirStrResult = luaTableMaybeGetString(L, index, "direction");
            if (!dirStrResult) {
                return std::unexpected("kind=longpress: field \"direction\" should be a valid direction string");
            }
            if (dirStrResult.value().has_value())
                direction = g_pTrackpadGestures->dirForString(dirStrResult.value().value());
        }
    } else if (kind == "pinch") {
        type = GestureType::PINCH;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin == 0)
            return std::unexpected("kind=pinch: fingers must be a positive integer");

        auto dirStrResult = luaTableGetString(L, index, "direction");
        if (!dirStrResult)
            return std::unexpected{std::format("kind=pinch: field \"direction\" must be a valid direction string")};
        direction = g_pTrackpadGestures->dirForString(dirStrResult.value());
        if (!hgGesture && !ShimTrackpadGestures::isSinglePinchDirection(direction)) {
            return std::unexpected("kind=pinch: direction must be pinchin/pinchout");
        }

        if (!ShimTrackpadGestures::isPinch(direction)) {
            if (hgGesture) {
                return std::unexpected("kind=pinch: direction must be pinch/pinchin/pinchout");
            } else {
                return std::unexpected("kind=pinch: direction must be pinchin/pinchout");
            }
        }

    } else if (!hgGesture && kind == "tap") {
        type = GestureType::TAP;

        fingersOrOrigin = luaTableGetInt(L, index, "fingers");
        if (fingersOrOrigin == 0)
            return std::unexpected("kind=tap: fingers must be a positive integer");
    } else {
        return std::unexpected(std::format("invalid gesture kind: {}", kind));
    }

    return GesturePattern{
        .type            = type,
        .direction       = direction,
        .fingersOrOrigin = fingersOrOrigin,
    };
}

int newGesture(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(
            L, "hyprgrass.gesture: expected argument to be a table {pattern, action, ...}"
        );

    GesturePattern gesture;
    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });
        lua_getfield(L, 1, "pattern");

        auto maybeGesture = gesturePatternFromTable(L, 2, true);
        if (!maybeGesture) {
            return Config::Lua::Bindings::Internal::configError(
                L, std::format("hyprgrass.gesture: in field \"pattern\": {}", maybeGesture.error())
            );
        }

        gesture = maybeGesture.value();
    }

    auto modResult = luaTableMaybeGetString(L, 1, "mod");
    if (!modResult) {
        return Config::Lua::Bindings::Internal::configError(L, "hyprgrass.gesture: {}", modResult.error());
    }

    auto maybeMod    = modResult.value();
    uint32_t modMask = maybeMod ? g_pKeybindManager->stringToModMask(std::string{maybeMod.value()}) : 0;

    auto maybeScaleResult = luaTableMaybeGetFloat(L, 1, "scale");
    if (!maybeScaleResult) {
        return Config::Lua::Bindings::Internal::configError(L, "hyprgrass.gesture: {}", maybeScaleResult.error());
    }
    float deltaScale = maybeScaleResult.value().value_or(1.f);
    if (deltaScale < 0.1f) {
        return Config::Lua::Bindings::Internal::configError(
            L, "hyprgrass.gesture: field \"scale\" must be at least 0.1"
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
                L, "hyprgrass.gesture: action must be a string (e.g. \"workspace\"), lua function, or "
                   "dispatcher (e.g. hl.dsp.focus(...))"
            );
        }
    }

    CTrackpadGestures* handler = g_pShimTrackpadGestures->get(gesture.type);
    std::expected<void, std::string> result;

    auto wsNameResult = luaTableMaybeGetString(L, 1, "workspace_name");
    if (!wsNameResult)
        return Config::Lua::Bindings::Internal::configError(L, wsNameResult.error());
    std::string workspaceName{wsNameResult.value().value_or("")};

    auto zoomLevelResult = luaTableMaybeGetString(L, 1, "zoom_level");
    if (!zoomLevelResult)
        return Config::Lua::Bindings::Internal::configError(L, zoomLevelResult.error());
    std::string zoomLevel{zoomLevelResult.value().value_or("")};

    auto modeResult = luaTableMaybeGetString(L, 1, "zoom_level");
    if (!modeResult)
        return Config::Lua::Bindings::Internal::configError(L, modeResult.error());
    std::string mode{modeResult.value().value_or("")};

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
                L, std::format("hyprgrass.gesture: unknown action \"{}\"", action)
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
    static const GestureType dragGestureTypes[3] = {
        GestureType::SWIPE,
        GestureType::LONG_PRESS,
        GestureType::EDGE_SWIPE,
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

std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchDownHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchUpHook;
std::shared_ptr<HOOK_CALLBACK_FN> g_pTouchMoveHook;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

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

    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->longPressDelay);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_config->edgeMargin);
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
