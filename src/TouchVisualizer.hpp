#include "src/devices/ITouch.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Shaders.hpp>
#include <utility>
#include <vector>

class Visualizer {
  public:
    void onRender();
    void damageAll();
    void damageFinger(uint64_t id);

    void onTouchDown(ITouch::SDownEvent);
    void onTouchUp(ITouch::SUpEvent);
    void onTouchMotion(ITouch::SMotionEvent);

  private:
    bool tempDamaged             = false;
    const int TOUCH_POINT_RADIUS = 15;
    std::vector<std::pair<uint64_t, Vector2D>> finger_positions;
    std::vector<std::pair<uint64_t, Vector2D>> prev_finger_positions;
};
