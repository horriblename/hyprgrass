#pragma once
#include "./gestures/Gestures.hpp"
#include "HyprLogger.hpp"
#include "VecSet.hpp"
#include "gestures/Shared.hpp"
#include <memory>

#define private public
#include <hyprland/src/debug/Log.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/includes.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>
#undef private

#include <wayfire/touch/touch.hpp>
#include <wayland-server-core.h>

class GestureManager : public IGestureManager {
  public:
    uint32_t long_press_next_trigger_time;
    std::vector<SP<SKeybind>> internalBinds;

    GestureManager();
    ~GestureManager();
    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchDown(ITouch::SDownEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchUp(ITouch::SUpEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchMove(ITouch::SMotionEvent e);

    void onLongPressTimeout(uint32_t time_msec);

    // workaround
    void touchBindDispatcher(std::string args);

  protected:
    SMonitorArea getMonitorArea() const override;
    bool handleCompletedGesture(const CompletedGestureEvent& gev) override;
    void handleCancelledGesture() override;

  private:
    VecSet<CWeakPointer<CWLTouchResource>> touchedResources;
    PHLMONITOR m_pLastTouchedMonitor;
    SMonitorArea m_sMonitorArea;
    wl_event_source* long_press_timer;
    bool hookHandled = false;
    wf::touch::point_t previousSwipePoint;
    struct {
        bool active = false;
        CCssGapData old_gaps_in;
    } resizeOnBorderInfo;

    bool handleGestureBind(std::string bind, bool pressed);

    // converts wlr touch event positions (number between 0.0 to 1.0) to pixel position,
    // takes into consideration monitor size and offset
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    // reverse of wlrTouchEventPositionAsPixels
    Vector2D pixelPositionToPercentagePosition(wf::touch::point_t) const;
    bool handleWorkspaceSwipe(const GestureDirection direction);
    void updateWorkspaceSwipe();
    void updateHookSwipe();

    bool handleDragGesture(const DragGestureEvent& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGestureEvent& gev) override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override;
    void stopLongPressTimer() override;

    void sendCancelEventsToWindows() override;
};

inline std::unique_ptr<GestureManager> g_pGestureManager;
