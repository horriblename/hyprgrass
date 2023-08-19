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

  protected:
    SMonitorArea getMonitorArea() const override;
    bool handleGesture(const CompletedGesture& gev) override;
    void handleCancelledGesture() override;

  private:
    std::vector<wlr_surface*> touchedSurfaces;
    bool m_bDispatcherFound = false;
    CMonitor* m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;

    // for workspace swipe
    wf::touch::point_t m_vGestureLastCenter;

    void addDefaultGestures();
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    void handleWorkspaceSwipe(const CompletedGesture& gev);

    void sendCancelEventsToWindows() override;
};

inline std::unique_ptr<CGestures> g_pGestureManager;
