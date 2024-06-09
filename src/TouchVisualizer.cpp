#include "TouchVisualizer.hpp"
#include "src/devices/ITouch.hpp"
#include "src/helpers/Region.hpp"
#include "src/macros.hpp"
#include "src/render/OpenGL.hpp"
#include "src/render/Renderer.hpp"
#include <cairo/cairo.h>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>

CBox boxAroundCenter(Vector2D center, double radius) {
    return CBox(center.x, center.y, radius, radius);
}

Visualizer::Visualizer() {
    this->cairoSurface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2 * TOUCH_POINT_RADIUS, 2 * TOUCH_POINT_RADIUS);
    auto cairo = cairo_create(this->cairoSurface);

    const double R = TOUCH_POINT_RADIUS;
    auto radpat    = cairo_pattern_create_radial(R, R, R, R, R, R);
    cairo_pattern_add_color_stop_rgba(radpat, 0, 1.0, 1.0, 1.0, 0.8);
    cairo_set_source(cairo, radpat);
    cairo_fill(cairo);

    cairo_pattern_destroy(radpat);
    cairo_destroy(cairo);
}

Visualizer::~Visualizer() {
    if (this->cairoSurface)
        cairo_surface_destroy(this->cairoSurface);
}

void Visualizer::onRender() {
    if (this->finger_positions.size() < 1) {
        return;
    }

    // FIXME: I am almost 100% certain this is wrong
    const auto monitor = g_pCompositor->m_pLastMonitor.get();
    const auto monSize = monitor->vecSize;

    const auto pos      = this->finger_positions[0] * monSize;
    const auto last_pos = this->prev_finger_positions[0] * monSize;
    auto dmg            = boxAroundCenter(pos, TOUCH_POINT_RADIUS);
    auto last_dmg       = boxAroundCenter(last_pos, TOUCH_POINT_RADIUS);

    CRegion damage = {pos.x - TOUCH_POINT_RADIUS, pos.y - TOUCH_POINT_RADIUS,
                      static_cast<double>(2 * TOUCH_POINT_RADIUS), static_cast<double>(2 * TOUCH_POINT_RADIUS)};

    g_pHyprRenderer->damageBox(&dmg);
    g_pHyprRenderer->damageBox(&last_dmg);

    // idk what this does
    g_pCompositor->scheduleFrameForMonitor(monitor);

    const auto data = cairo_image_surface_get_data(this->cairoSurface);

    // no deallocate???
    this->texture.allocate();
    glBindTexture(GL_TEXTURE_2D, this->texture.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, monSize.x, monSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    g_pHyprOpenGL->renderTexture(this->texture, &dmg, 1.f, 0, true);
}

void Visualizer::onTouchDown(ITouch::SDownEvent ev) {
    this->finger_positions.emplace(ev.touchID, ev.pos);
    this->prev_finger_positions.emplace(std::pair(ev.touchID, ev.pos));
}

void Visualizer::onTouchUp(ITouch::SUpEvent ev) {
    this->finger_positions.erase(ev.touchID);
    this->prev_finger_positions.erase(ev.touchID);
}

void Visualizer::onTouchMotion(ITouch::SMotionEvent ev) {
    this->prev_finger_positions[ev.touchID] = this->finger_positions[ev.touchID];
    this->finger_positions[ev.touchID]      = ev.pos;
}

void Visualizer::damageAll() {
    CBox dm;
    for (const auto& point : this->prev_finger_positions) {
        dm = CBox{point.second.x - TOUCH_POINT_RADIUS, point.second.y - TOUCH_POINT_RADIUS,
                  static_cast<double>(2 * TOUCH_POINT_RADIUS), static_cast<double>(2 * TOUCH_POINT_RADIUS)};
        g_pHyprRenderer->damageBox(&dm);
    }
}

void Visualizer::damageFinger(int32_t id) {
    for (const auto& point : this->prev_finger_positions) {
        if (point.first != id) {
            continue;
        }

        CBox dm = {point.second.x - TOUCH_POINT_RADIUS, point.second.y - TOUCH_POINT_RADIUS,
                   static_cast<double>(2 * TOUCH_POINT_RADIUS), static_cast<double>(2 * TOUCH_POINT_RADIUS)};
        g_pHyprRenderer->damageBox(&dm);
        return;
    }

    RASSERT(false, "Visualizer tried to damage non-existent finger id {}", id)
}
