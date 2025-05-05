#include "TouchVisualizer.hpp"
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>

CBox boxAroundCenter(Vector2D center, double radius) {
    return CBox(center.x - radius, center.y - radius, 2 * radius, 2 * radius);
}

Visualizer::Visualizer() {
    const int R = TOUCH_POINT_RADIUS;

    this->cairoSurface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2 * TOUCH_POINT_RADIUS, 2 * TOUCH_POINT_RADIUS);
    auto cairo = cairo_create(cairoSurface);

    cairo_arc(cairo, R, R, R, 0, 2 * PI);
    cairo_set_source_rgba(cairo, 0.8, 0.8, 0.1, 0.6);
    cairo_fill(cairo);

    cairo_destroy(cairo);

    const unsigned char* data = cairo_image_surface_get_data(this->cairoSurface);

    this->texture->allocate();
    glBindTexture(GL_TEXTURE_2D, this->texture->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2 * TOUCH_POINT_RADIUS, 2 * TOUCH_POINT_RADIUS, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
}

Visualizer::~Visualizer() {
    if (this->cairoSurface)
        cairo_surface_destroy(this->cairoSurface);
}

void Visualizer::onPreRender() {}

void Visualizer::onRender() {
    if (this->finger_positions.size() < 1) {
        return;
    }

    const auto monitor = g_pCompositor->m_lastMonitor.lock();

    // HACK: should not damage monitor, however, I don't understand jackshit
    // about damage so here we are.
    // If you know how to do damage properly I BEG OF YOU PLEASE ABSOLVE ME
    // OF MY SINS
    if (this->finger_positions.size()) {
        g_pHyprRenderer->damageMonitor(monitor);
    }

    for (auto& finger : this->finger_positions) {
        CBox dmg = boxAroundCenter(finger.second.curr, TOUCH_POINT_RADIUS);
        g_pHyprOpenGL->renderTexture(this->texture, dmg, 1.f, 0, true);
    }
}

void Visualizer::onTouchDown(ITouch::SDownEvent ev) {
    auto mon = g_pCompositor->m_lastMonitor.lock();
    this->finger_positions.emplace(ev.touchID, FingerPos{ev.pos * mon->m_pixelSize + mon->m_position, std::nullopt});
    g_pCompositor->scheduleFrameForMonitor(mon);
}

void Visualizer::onTouchUp(ITouch::SUpEvent ev) {
    this->damageFinger(ev.touchID);
    this->finger_positions.erase(ev.touchID);
    g_pCompositor->scheduleFrameForMonitor(g_pCompositor->m_lastMonitor.lock());
}

void Visualizer::onTouchMotion(ITouch::SMotionEvent ev) {
    auto mon                           = g_pCompositor->m_lastMonitor.lock();
    this->finger_positions[ev.touchID] = {ev.pos * mon->m_pixelSize + mon->m_position, std::nullopt};
    g_pCompositor->scheduleFrameForMonitor(mon);
}

void Visualizer::damageFinger(int32_t id) {
    auto finger = this->finger_positions.at(id);

    CBox dm = boxAroundCenter(finger.curr, TOUCH_POINT_RADIUS);
    g_pHyprRenderer->damageBox(dm);

    if (finger.last_rendered.has_value()) {
        dm = boxAroundCenter(finger.last_rendered.value(), TOUCH_POINT_RADIUS);
        g_pHyprRenderer->damageBox(dm);
    }
}
