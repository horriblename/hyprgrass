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

    void begin(const STrackpadGestureBegin& e) override;

    void update(const STrackpadGestureUpdate& e) override;

    void end(const STrackpadGestureEnd& e) override;

  private:
    int startRef;
    int updateRef;
    int endRef;

    std::chrono::steady_clock::time_point last_updated{};
};
