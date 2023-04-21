#pragma once
#include "globals.hpp"
#include <src/includes.hpp>
#include <src/managers/input/InputManager.hpp>

struct SFingerData {
    uint8_t type; // TODO enum: down, move, up
    uint8_t id;
    uint32_t time; // time of last update
    Vector2D pos;
};

class CGestures {
  public:
    CGestures();
    void onTouchDown(wlr_touch_down_event*);
    void onTouchUp(wlr_touch_up_event*);
    void onTouchMove(wlr_touch_motion_event*);

  private:
    std::list<SFingerData> m_lFingers;
    Vector2D m_vTouchGestureLastCenter;
    bool m_bTouchGestureActive;
};
