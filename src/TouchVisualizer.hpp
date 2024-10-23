#include "src/devices/ITouch.hpp"
#include "src/render/Texture.hpp"
#include <cairo/cairo.h>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Shaders.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <optional>
#include <unordered_map>

struct FingerPos {
    Vector2D curr;
    std::optional<Vector2D> last_rendered;
    std::optional<Vector2D> last_rendered2;
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
