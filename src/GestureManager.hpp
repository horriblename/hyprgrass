#pragma once
#include "./gestures/Gestures.hpp"
#include "globals.hpp"
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <optional>
#include <vector>
#include <wayfire/touch/touch.hpp>

enum class DragActionType {
    WORKSPACE_SWIPE,
    MOVE_WINDOW,
};

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

    int holdTimerCallback();

  protected:
    SMonitorArea getMonitorArea() const override;
    void handleGesture(const CompletedGesture& gev) override;
    void handleCancelledGesture() override{};

  private:
    bool m_bDispatcherFound = false;
    CMonitor* m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;
    wl_event_source* m_pHoldTimer;
    uint32_t last_touch_down_time = 0;

    // for workspace swipe
    std::optional<DragActionType> active_drag_action = std::nullopt;
    wf::touch::point_t m_vGestureLastCenter;

    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    void handleWorkspaceSwipe(const CompletedGesture& gev);
    void handleHoldGesture(const CompletedGesture& gev);

    void moveWindowBegin();
    void moveWindowEnd();
    void moveWindowUpdate();

    void reset_hold_event_timer();
};

inline std::unique_ptr<CGestures> g_pGestureManager;
