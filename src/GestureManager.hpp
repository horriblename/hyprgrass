#pragma once
#include "gestures/Gestures.hpp"
#include "globals.hpp"
#include "src/helpers/Monitor.hpp"
#include <src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

/**
 * Represents a touch gesture.
 *
 * Finger count can be arbitrary (might be a good idea to limit to >3)
 */
struct TouchGesture {
    eTouchGestureType type;
    gestureDirection direction;
    int finger_count;

    std::string to_string() const;
};

class CGestures : public IGestureManager {
  public:
    CGestures();
    bool onTouchDown(wlr_touch_down_event*) override;
    bool onTouchUp(wlr_touch_up_event*) override;
    bool onTouchMove(wlr_touch_motion_event*) override;

    void emulateSwipeBegin(uint32_t time);
    void emulateSwipeEnd(uint32_t time, bool cancelled);
    void emulateSwipeUpdate(uint32_t time);

    void handleGesture(const TouchGesture& gev);
    // TODO how to refer to gesture?
    // void deleteTouchGesture()

  protected:
    std::optional<SMonitorArea> getMonitorArea() const override;

  private:
    // std::vector<std::unique_ptr<wf::touch::gesture_t>> m_vGestures;
    // wf::touch::gesture_state_t m_sGestureState;

    // Vector2D m_vTouchGestureLastCenter;
    bool m_bWorkspaceSwipeActive = false;
    bool m_bDispatcherFound      = false;
    wf::touch::point_t m_vGestureLastCenter;

    CMonitor* m_pLastTouchedMonitor;

    void addDefaultGestures();
};

inline std::unique_ptr<CGestures> g_pGestureManager;
