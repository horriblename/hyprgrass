#pragma once
#include "gestures/Gestures.hpp"
#include "globals.hpp"
#include "src/debug/Log.hpp"
#include "src/helpers/Monitor.hpp"
#include <src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

class CGestures : public IGestureManager {
  public:
    CGestures();
    bool onTouchDown(wlr_touch_down_event*);
    bool onTouchUp(wlr_touch_up_event*);
    bool onTouchMove(wlr_touch_motion_event*);

    void emulateSwipeBegin(uint32_t time);
    void emulateSwipeEnd(uint32_t time, bool cancelled);
    void emulateSwipeUpdate(uint32_t time);

    // TODO how to refer to gesture?
    // void deleteTouchGesture()

  protected:
    SMonitorArea getMonitorArea() const override;
    void handleGesture(const TouchGesture& gev) override;
    void handleCancelledGesture() override {
        // DEBUG
        Debug::log(INFO, "gesture cancelled, last progress: %.2f",
                   debug_gestureProgressBeforeUpdate);
    };

  private:
    // std::vector<std::unique_ptr<wf::touch::gesture_t>> m_vGestures;
    // wf::touch::gesture_state_t m_sGestureState;

    // Vector2D m_vTouchGestureLastCenter;
    bool m_bWorkspaceSwipeActive = false;
    bool m_bDispatcherFound      = false;
    SMonitorArea m_sMonitorArea;
    wf::touch::point_t m_vGestureLastCenter;

    CMonitor* m_pLastTouchedMonitor;

    void addDefaultGestures();
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
};

inline std::unique_ptr<CGestures> g_pGestureManager;
