#pragma once

#include <hyprutils/utils/ScopeGuard.hpp>
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "GestureManager.hpp"
#define private public
#include "config/lua/ConfigManager.hpp"
#include "src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"
#undef private

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

        lua_createtable(L, 0, 2); // local pos = {}
        auto state         = g_pGestureManager->getGestureState();
        auto origin_center = state.get_center().origin;
        lua_pushnumber(L, origin_center.x);
        lua_setfield(L, -2, "x"); // pos.x = (origin_center.x)
        lua_pushnumber(L, origin_center.y);
        lua_setfield(L, -2, "y"); // pos.y = (origin_center.x)

        // callback(pos)
        if (lua_pcall(L, 1, 0, this->startRef) != LUA_OK) {
            Log::logger->log(Log::ERR, "[hyprgrass] start function failed: {}", lua_tostring(L, -1));
        }
    };

    void update(const STrackpadGestureUpdate& e) override {
        if (updateRef == LUA_NOREF)
            return;

        auto mgr     = Config::Lua::mgr();
        lua_State* L = mgr->m_lua;
        int base     = lua_gettop(L);
        Hyprutils::Utils::CScopeGuard x([=] { lua_settop(L, base); });

        lua_rawgeti(L, LUA_REGISTRYINDEX, this->updateRef);

        lua_createtable(L, 0, 2); // local pos = {}
        auto state  = g_pGestureManager->getGestureState();
        auto center = state.get_center().current;
        lua_pushnumber(L, center.x);
        lua_setfield(L, -2, "x"); // pos.x = (center.x)
        lua_pushnumber(L, center.y);
        lua_setfield(L, -2, "y"); // pos.y = (center.y)

        // callback(pos)
        if (lua_pcall(L, 1, 0, this->updateRef) != LUA_OK) {
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

        lua_createtable(L, 0, 2); // local pos = {}
        auto state  = g_pGestureManager->getGestureState();
        auto center = state.get_center().current;
        lua_pushnumber(L, center.x);
        lua_setfield(L, -2, "x"); // pos.x = (center.x)
        lua_pushnumber(L, center.y);
        lua_setfield(L, -2, "y"); // pos.y = (center.y)

        // callback(pos)
        if (lua_pcall(L, 1, 0, this->endRef) != LUA_OK) {
            Log::logger->log(Log::ERR, "[hyprgrass] end function failed: {}", lua_tostring(L, -1));
        }
    };

  private:
    int startRef;
    int updateRef;
    int endRef;
};
