#pragma once
#include "./gestures/Gestures.hpp"
#include "globals.hpp"
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

class CGestures : public IGestureManager {
  public:
    CGestures();
    // @return whether a gesture was triggered after this event
    bool onTouchDown(wlr_touch_down_event*);

    // @return whether a gesture was triggered after this event
    bool onTouchUp(wlr_touch_up_event*);

    // @return whether a gesture was triggered after this event
    bool onTouchMove(wlr_touch_motion_event*);

    void emulateSwipeBegin(uint32_t time);
    void emulateSwipeEnd(uint32_t time, bool cancelled);
    void emulateSwipeUpdate(uint32_t time);

    std::vector<int> getAllFingerIds();

  protected:
    SMonitorArea getMonitorArea() const override;
    void handleGesture(const TouchGesture& gev) override;
    void handleCancelledGesture() override {
        // DEBUG
        Debug::log(INFO, "gesture cancelled, last progress: %.2f",
                   debug_gestureProgressBeforeUpdate);
    };

  private:
    bool m_bDispatcherFound = false;
    CMonitor* m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;

    // for workspace swipe
    bool m_bWorkspaceSwipeActive = false;
    wf::touch::point_t m_vGestureLastCenter;

    void addDefaultGestures();
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
};

inline std::unique_ptr<CGestures> g_pGestureManager;
