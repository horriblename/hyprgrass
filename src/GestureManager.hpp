#pragma once
#include "./gestures/Gestures.hpp"
#include "gestures/Shared.hpp"
#include "globals.hpp"
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <vector>
#include <wayfire/touch/touch.hpp>
#include <wayland-server-core.h>

class GestureManager : public IGestureManager {
  public:
    uint32_t long_press_next_trigger_time;
    GestureManager();
    ~GestureManager();
    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchDown(wlr_touch_down_event*);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchUp(wlr_touch_up_event*);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchMove(wlr_touch_motion_event*);

    void onLongPressTimeout(uint32_t time_msec);

  protected:
    SMonitorArea getMonitorArea() const override;
    bool handleCompletedGesture(const CompletedGesture& gev) override;
    void handleCancelledGesture() override;

  private:
    std::vector<wlr_surface*> touchedSurfaces;
    CMonitor* m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;
    wl_event_source* long_press_timer;

    // for workspace swipe
    wf::touch::point_t m_vGestureLastCenter;
    void emulateSwipeBegin(uint32_t time);
    void emulateSwipeEnd(uint32_t time, bool cancelled);
    void emulateSwipeUpdate(uint32_t time);

    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    bool handleWorkspaceSwipe(const GestureDirection direction);

    bool handleDragGesture(const DragGesture& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGesture& gev) override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override;
    void stopLongPressTimer() override;

    void sendCancelEventsToWindows() override;
};

inline std::unique_ptr<GestureManager> g_pGestureManager;
