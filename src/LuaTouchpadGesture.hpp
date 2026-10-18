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
    LuaTouchpadGesture(
        GestureType gestureType, Config::Lua::CLuaRef&& start, Config::Lua::CLuaRef&& update, Config::Lua::CLuaRef&& end
    )
        : gestureType(gestureType), startFn(std::move(start)), updateFn(std::move(update)), endFn(std::move(end)) {}

    void begin(const STrackpadGestureBegin& e) override;

    void update(const STrackpadGestureUpdate& e) override;

    void end(const STrackpadGestureEnd& e) override;

  private:
    GestureType gestureType;
    Config::Lua::CLuaRef startFn;
    Config::Lua::CLuaRef updateFn;
    Config::Lua::CLuaRef endFn;

    // for computing the rotation delta between updates
    double lastRotation = 0;
};
