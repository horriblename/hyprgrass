#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <cairo/cairo.h>

struct FingerPos {
    Vector2D curr;
    std::optional<Vector2D> last_rendered;
};

class Visualizer {
  public:
    Visualizer();
    ~Visualizer();
    void onPreRender();
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
    std::unordered_map<int32_t, FingerPos> finger_positions;
};
