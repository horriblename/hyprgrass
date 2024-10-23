#include "src/devices/ITouch.hpp"
#include "src/render/Texture.hpp"
#include <cairo/cairo.h>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Shaders.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <unordered_map>

class Visualizer {
  public:
    Visualizer();
    ~Visualizer();
    void onRender();
    void damageFinger(int32_t id);

    void onTouchDown(ITouch::SDownEvent);
    void onTouchUp(ITouch::SUpEvent);
    void onTouchMotion(ITouch::SMotionEvent);

  private:
    SP<CTexture> texture = makeShared<CTexture>();
    cairo_surface_t* cairoSurface;
    bool tempDamaged             = false;
    const int TOUCH_POINT_RADIUS = 30;
    std::unordered_map<int32_t, Vector2D> finger_positions;
    std::unordered_map<int32_t, Vector2D> prev_finger_positions;
};
