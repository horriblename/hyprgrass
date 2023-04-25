#pragma once
#include "globals.hpp"
#include "src/helpers/Monitor.hpp"
#include <glm/glm.hpp>
#include <src/Compositor.hpp>
#include <src/includes.hpp>
#include <src/managers/input/InputManager.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

// Swipe params
constexpr static int EDGE_SWIPE_THRESHOLD = 10;
constexpr static double MIN_SWIPE_DISTANCE = 30;
constexpr static double MAX_SWIPE_DISTANCE = 450;
constexpr static double SWIPE_INCORRECT_DRAG_TOLERANCE = 150;

// Pinch params
constexpr static double PINCH_INCORRECT_DRAG_TOLERANCE = 200;
constexpr static double PINCH_THRESHOLD = 1.5;

// General
constexpr static double GESTURE_INITIAL_TOLERANCE = 40;
constexpr static uint32_t GESTURE_BASE_DURATION = 400;

enum eTouchGestureType {
    // Invalid Gesture
    GESTURE_TYPE_NONE,
    GESTURE_TYPE_SWIPE,
    GESTURE_TYPE_EDGE_SWIPE,
    // GESTURE_TYPE_PINCH,
};

enum eTouchGestureDirection {
    /* Swipe-specific */
    GESTURE_DIRECTION_LEFT = (1 << 0),
    GESTURE_DIRECTION_RIGHT = (1 << 1),
    GESTURE_DIRECTION_UP = (1 << 2),
    GESTURE_DIRECTION_DOWN = (1 << 3),
    /* Pinch-specific */
    // GESTURE_DIRECTION_IN = (1 << 4),
    // GESTURE_DIRECTION_OUT = (1 << 5),
};

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
    int finger_count = 0;

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

    void addTouchGesture(wf::touch::gesture_t gesture);
    void handleGesture(const TouchGesture& gev);
    // TODO how to refer to gesture?
    // void deleteTouchGesture()

  private:
    std::vector<std::unique_ptr<wf::touch::gesture_t>> m_pGestures;
    std::unique_ptr<wf::touch::gesture_t> m_pGestureHandler;
    std::unique_ptr<wf::touch::gesture_state_t> m_pGestureState;
    Vector2D m_vTouchGestureLastCenter;
    bool m_bTouchGestureActive;

    std::unique_ptr<wf::touch::gesture_t> m_pMultiSwipe, m_pEdgeSwipe;

    CMonitor* m_pLastTouchedMonitor;

    void updateGestures(const wf::touch::gesture_event_t&);
    uint32_t find_swipe_edges(wf::touch::point_t point);
};

inline std::unique_ptr<CGestures> g_pGestureManager;
