#include "TouchVisualizer.hpp"
#include "src/devices/ITouch.hpp"
#include "src/macros.hpp"
#include "src/render/Renderer.hpp"
#include <hyprland/src/SharedDefs.hpp>

void Visualizer::onRender() {
    if (this->finger_positions.size() < 1) {
        return;
    }

    // this->finger_positions
}

void Visualizer::onTouchDown(ITouch::SDownEvent ev) {
    this->finger_positions.push_back(std::pair(ev.touchID, ev.pos));
    this->prev_finger_positions.push_back(std::pair(ev.touchID, ev.pos));
}

void Visualizer::onTouchUp(ITouch::SUpEvent ev) {
    for (auto iter = this->prev_finger_positions.begin(); iter != this->prev_finger_positions.end(); iter++) {
        this->prev_finger_positions.erase(iter);
    }

    for (auto iter = this->finger_positions.begin(); iter != this->finger_positions.end(); iter++) {
        this->finger_positions.erase(iter);
    }
}

void Visualizer::onTouchMotion(ITouch::SMotionEvent ev) {
    for (auto& pair : this->finger_positions) {
        if (pair.first != ev.touchID)
            continue;

        for (auto& prev : this->prev_finger_positions) {
            if (prev.first != ev.touchID)
                continue;

            prev.second = pair.second;
            pair.second = ev.pos;

            return;
        }
        break;
    }
}

void Visualizer::damageAll() {
    CBox dm;
    for (const auto& point : this->prev_finger_positions) {
        dm = CBox{point.second.x - TOUCH_POINT_RADIUS, point.second.y - TOUCH_POINT_RADIUS,
                  static_cast<double>(2 * TOUCH_POINT_RADIUS), static_cast<double>(2 * TOUCH_POINT_RADIUS)};
        g_pHyprRenderer->damageBox(&dm);
    }
}

void Visualizer::damageFinger(uint64_t id) {
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
