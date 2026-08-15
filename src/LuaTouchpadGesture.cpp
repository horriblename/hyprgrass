#include "LuaTouchpadGesture.hpp"

// pushes a lua table with monitor data onto the stack
static void pushMonitorInfo(lua_State* L) {
    lua_createtable(L, 0, 2); // local monitor = {}
    const auto monArea = g_pGestureManager->getMonitorArea();

    lua_pushnumber(L, monArea.w);
    lua_setfield(L, -2, "width"); // monitor.width = (monArea.w)
    lua_pushnumber(L, monArea.h);
    lua_setfield(L, -2, "height"); // monitor.height = (monArea.h)
}

// pushes a {x, y} table onto the stack
static void pushVec2(lua_State* L, const Vector2D& vec) {
    lua_createtable(L, 0, 2); // local delta = {}
    lua_pushnumber(L, vec.x);
    lua_setfield(L, -2, "x"); // delta.x = (vec.x)
    lua_pushnumber(L, vec.y);
    lua_setfield(L, -2, "y"); // delta.y = (vec.y)
}

// pushes the fields common to the start and update tables onto the stack,
// expecting the table to already be on top of the stack
template <typename T> static void pushGestureInfo(lua_State* L, GestureType type, const T& e, double& lastRotation) {
    lua_pushstring(L, stringifyGestureType(type).c_str());
    lua_setfield(L, -2, "type"); // opt.type = "swipe"|"edge"|"longpress"|"pinch"

    if (e.swipe) {
        lua_pushinteger(L, e.swipe->timeMs);
        lua_setfield(L, -2, "time_ms"); // opt.time_ms = (e.swipe->timeMs)
        lua_pushinteger(L, e.swipe->fingers);
        lua_setfield(L, -2, "fingers"); // opt.fingers = (e.swipe->fingers)
        pushVec2(L, e.swipe->delta);
        lua_setfield(L, -2, "delta"); // opt.delta = delta
    } else if (e.pinch) {
        lua_pushinteger(L, e.pinch->timeMs);
        lua_setfield(L, -2, "time_ms"); // opt.time_ms = (e.pinch->timeMs)
        lua_pushinteger(L, e.pinch->fingers);
        lua_setfield(L, -2, "fingers"); // opt.fingers = (e.pinch->fingers)
        pushVec2(L, e.pinch->delta);
        lua_setfield(L, -2, "delta"); // opt.delta = delta
        lua_pushnumber(L, e.pinch->scale);
        lua_setfield(L, -2, "scale"); // opt.scale = (e.pinch->scale)
        const auto rotation = e.pinch->rotation;
        lua_pushnumber(L, rotation - lastRotation);
        lua_setfield(L, -2, "rotation"); // opt.rotation = rotation relative to the last update
        lastRotation = rotation;
    }
}

void LuaTouchpadGesture::begin(const STrackpadGestureBegin& e) {
    if (startRef == LUA_NOREF)
        return;

    auto mgr     = Config::Lua::mgr();
    lua_State* L = mgr->m_lua;
    int base     = lua_gettop(L);
    Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

    lua_rawgeti(L, LUA_REGISTRYINDEX, this->startRef);

    lua_createtable(L, 0, 8); // local opt = {}

    lastRotation = 0;
    pushGestureInfo(L, gestureType, e, lastRotation);

    {
        lua_createtable(L, 0, 2); // local pos = {}
        auto state         = g_pGestureManager->getGestureState();
        auto origin_center = state.get_center().origin;
        lua_pushnumber(L, origin_center.x);
        lua_setfield(L, -2, "x"); // pos.x = (origin_center.x)
        lua_pushnumber(L, origin_center.y);
        lua_setfield(L, -2, "y"); // pos.y = (origin_center.y)

        lua_setfield(L, -2, "pos"); // opt.pos = pos
    }

    pushMonitorInfo(L);
    lua_setfield(L, -2, "monitor"); // opt.monitor = monitor

    int result = mgr->guardedPCall(
        1, 0, 0, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hyprgrass.gesture: start()"
    );
    if (result != LUA_OK) {
        Log::logger->log(Log::ERR, "[hyprgrass] start function failed: {}", lua_tostring(L, -1));
    }
}

void LuaTouchpadGesture::update(const STrackpadGestureUpdate& e) {
    if (updateRef == LUA_NOREF)
        return;

    auto now = std::chrono::steady_clock::now();
    if (now - last_updated < std::chrono::milliseconds{Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS})
        return;

    last_updated = now;

    auto mgr     = Config::Lua::mgr();
    lua_State* L = mgr->m_lua;
    int base     = lua_gettop(L);
    Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

    lua_rawgeti(L, LUA_REGISTRYINDEX, this->updateRef);

    lua_createtable(L, 0, 6); // local opt = {}

    pushGestureInfo(L, gestureType, e, lastRotation);

    int result = mgr->guardedPCall(
        1, 0, 0, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hyprgrass.gesture: update()"
    );
    if (result != LUA_OK) {
        Log::logger->log(Log::ERR, "[hyprgrass] update function failed: {}", lua_tostring(L, -1));
    }
}

void LuaTouchpadGesture::end(const STrackpadGestureEnd& e) {
    if (endRef == LUA_NOREF)
        return;

    auto mgr     = Config::Lua::mgr();
    lua_State* L = mgr->m_lua;
    int base     = lua_gettop(L);
    Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

    lua_rawgeti(L, LUA_REGISTRYINDEX, this->endRef);

    lua_createtable(L, 0, 3); // local opt = {}

    lua_pushstring(L, stringifyGestureType(gestureType).c_str());
    lua_setfield(L, -2, "type"); // opt.type = "swipe"|"edge"|"longpress"|"pinch"

    if (e.swipe) {
        lua_pushinteger(L, e.swipe->timeMs);
        lua_setfield(L, -2, "time_ms"); // opt.time_ms = (e.swipe->timeMs)
        lua_pushboolean(L, e.swipe->cancelled);
        lua_setfield(L, -2, "cancelled"); // opt.cancelled = (e.swipe->cancelled)
    } else if (e.pinch) {
        lua_pushinteger(L, e.pinch->timeMs);
        lua_setfield(L, -2, "time_ms"); // opt.time_ms = (e.pinch->timeMs)
        lua_pushboolean(L, e.pinch->cancelled);
        lua_setfield(L, -2, "cancelled"); // opt.cancelled = (e.pinch->cancelled)
    }

    int result = mgr->guardedPCall(
        1, 0, 0, Config::Lua::CConfigManager::LUA_TIMEOUT_EVENT_CALLBACK_MS, "hyprgrass.gesture: finish()"
    );
    if (result != LUA_OK) {
        Log::logger->log(Log::ERR, "[hyprgrass] finish function failed: {}", lua_tostring(L, -1));
    }
}
