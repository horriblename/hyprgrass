#include <hyprutils/utils/ScopeGuard.hpp>
#include <stdexcept>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "GestureManager.hpp"
#define private public
#include "config/lua/ConfigManager.hpp"
#include "src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"
#undef private

static constexpr uint32_t TIMEOUT_MSEC = 80;
// targeting 60 updates per second
static constexpr uint32_t UPDATE_INTERVAL_MSEC = 1000 / 60;

// pushes a lua table with monitor data onto the stack
static void pushMonitorInfo(lua_State* L) {
    lua_createtable(L, 0, 2); // local monitor = {}
    const auto monArea = g_pGestureManager->getMonitorArea();

    lua_pushnumber(L, monArea.w);
    lua_setfield(L, -2, "width"); // monitor.width = (monArea.w)
    lua_pushnumber(L, monArea.h);
    lua_setfield(L, -2, "height"); // monitor.height = (monArea.h)
}

class LuaTouchpadGesture : public ITrackpadGesture {
  public:
    LuaTouchpadGesture(int startRef, int updateRef, int endRef)
        : startRef(startRef), updateRef(updateRef), endRef(endRef) {}

    void begin(const STrackpadGestureBegin& e) override {
        if (startRef == LUA_NOREF)
            return;

        auto mgr     = Config::Lua::mgr();
        lua_State* L = mgr->m_lua;
        int base     = lua_gettop(L);
        Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

        lua_rawgeti(L, LUA_REGISTRYINDEX, this->startRef);

        lua_createtable(L, 0, 2); // local opt = {}

        {
            lua_createtable(L, 0, 2); // local pos = {}
            auto state         = g_pGestureManager->getGestureState();
            auto origin_center = state.get_center().origin;
            lua_pushnumber(L, origin_center.x);
            lua_setfield(L, -2, "x"); // pos.x = (origin_center.x)
            lua_pushnumber(L, origin_center.y);
            lua_setfield(L, -2, "y"); // pos.y = (origin_center.x)

            lua_setfield(L, -2, "pos"); // opt.pos = pos
        }

        pushMonitorInfo(L);
        lua_setfield(L, -2, "monitor"); // opt.monitor = monitor

        int result = mgr->guardedPCall(1, 0, 0, TIMEOUT_MSEC, "hyprgrass.gesture: start()");
        if (result != LUA_OK) {
            Log::logger->log(Log::ERR, "[hyprgrass] start function failed: {}", lua_tostring(L, -1));
        }
    };

    void update(const STrackpadGestureUpdate& e) override {
        if (updateRef == LUA_NOREF)
            return;

        auto now = std::chrono::steady_clock::now();
        if (now - last_updated < std::chrono::milliseconds{UPDATE_INTERVAL_MSEC})
            return;

        last_updated = now;

        auto mgr     = Config::Lua::mgr();
        lua_State* L = mgr->m_lua;
        int base     = lua_gettop(L);
        Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

        lua_rawgeti(L, LUA_REGISTRYINDEX, this->updateRef);

        lua_createtable(L, 0, 3); // local opt = {}

        {
            lua_createtable(L, 0, 2); // local pos = {}
            auto state  = g_pGestureManager->getGestureState();
            auto center = state.get_center().current;
            lua_pushnumber(L, center.x);
            lua_setfield(L, -2, "x"); // pos.x = (center.x)
            lua_pushnumber(L, center.y);
            lua_setfield(L, -2, "y"); // pos.y = (center.y)

            lua_setfield(L, -2, "pos"); // opt.pos = pos
        }

        pushMonitorInfo(L);
        lua_setfield(L, -2, "monitor"); // opt.monitor = monitor

        auto time_msec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        lua_pushinteger(L, time_msec);
        lua_setfield(L, -2, "time_msec"); // opt.time_msec = (time)

        int result = mgr->guardedPCall(1, 0, 0, TIMEOUT_MSEC, "hyprgrass.gesture: update()");
        if (result != LUA_OK) {
            Log::logger->log(Log::ERR, "[hyprgrass] update function failed: {}", lua_tostring(L, -1));
        }
    }

    void end(const STrackpadGestureEnd& e) override {
        if (endRef == LUA_NOREF)
            return;

        auto mgr     = Config::Lua::mgr();
        lua_State* L = mgr->m_lua;
        int base     = lua_gettop(L);
        Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

        lua_rawgeti(L, LUA_REGISTRYINDEX, this->endRef);

        lua_createtable(L, 0, 2); // local opt = {}

        {
            lua_createtable(L, 0, 2); // local pos = {}
            auto state  = g_pGestureManager->getGestureState();
            auto center = state.get_center().current;
            lua_pushnumber(L, center.x);
            lua_setfield(L, -2, "x"); // pos.x = (center.x)
            lua_pushnumber(L, center.y);
            lua_setfield(L, -2, "y"); // pos.y = (center.y)

            lua_setfield(L, -2, "pos"); // opt.pos = pos
        }

        pushMonitorInfo(L);
        lua_setfield(L, -2, "monitor"); // opt.monitor = monitor

        int result = mgr->guardedPCall(1, 0, 0, TIMEOUT_MSEC, "hyprgrass.gesture: finish()");
        if (result != LUA_OK) {
            Log::logger->log(Log::ERR, "[hyprgrass] finish function failed: {}", lua_tostring(L, -1));
        }
    }

  private:
    int startRef;
    int updateRef;
    int endRef;

    std::chrono::steady_clock::time_point last_updated{};
};
