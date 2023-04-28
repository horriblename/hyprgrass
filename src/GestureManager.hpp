#pragma once
#include "Gestures.hpp"
#include "globals.hpp"
#include "src/helpers/Monitor.hpp"
#include <glm/glm.hpp>
#include <src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

// can be one of @eTouchGestureDirection or a combination of them
using gestureDirection = uint32_t;

/**
 * Represents a touch gesture.
 *
 * Finger count can be arbitrary (might be a good idea to limit to >3)
 */
struct TouchGesture {
    eTouchGestureType type;
    gestureDirection direction;
    int finger_count;
};

class CMultiAction : public wf::touch::gesture_action_t {
  public:
    CMultiAction(double threshold) : threshold(threshold){};
    // bool pinch;
    // bool last_pinch_was_pinch_in = false;
    double threshold;

    gestureDirection target_direction = 0;
    int finger_count                  = 0;

    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state,
                 const wf::touch::gesture_event_t& event) override;

    void reset(uint32_t time) override {
        gesture_action_t::reset(time);
        target_direction = 0;
    };
};

class CGestures {
  public:
    CGestures();
    bool onTouchDown(wlr_touch_down_event*);
    bool onTouchUp(wlr_touch_up_event*);
    bool onTouchMove(wlr_touch_motion_event*);

    void emulateSwipeBegin();
    void emulateSwipeEnd();

    void addTouchGesture(std::unique_ptr<wf::touch::gesture_t> gesture);
    void handleGesture(const TouchGesture& gev);
    // TODO how to refer to gesture?
    // void deleteTouchGesture()

  private:
    std::vector<std::unique_ptr<wf::touch::gesture_t>> m_pGestures;
    wf::touch::gesture_state_t m_pGestureState;
    Vector2D m_vTouchGestureLastCenter;
    bool m_bTouchGestureActive;

    CMonitor* m_pLastTouchedMonitor;

    void addDefaultGestures();
    void updateGestures(const wf::touch::gesture_event_t&);
    uint32_t find_swipe_edges(wf::touch::point_t point);
};

inline std::unique_ptr<CGestures> g_pGestureManager;
