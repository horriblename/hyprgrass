#pragma once
#include "WorkspaceSwipe.hpp"
#include "gestures/Gestures.hpp"
#include "globals.hpp"
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>

class CGestures : public IGestureManager {
  public:
    CGestures();
    bool onTouchDown(wlr_touch_down_event*);
    bool onTouchUp(wlr_touch_up_event*);
    bool onTouchMove(wlr_touch_motion_event*);

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
    bool m_bDispatcherFound = false;
    SMonitorArea m_sMonitorArea;
    WorkspaceSwipeManager workspaceSwipe;

    CMonitor* m_pLastTouchedMonitor;

    void addDefaultGestures();
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
};

inline std::unique_ptr<CGestures> g_pGestureManager;
