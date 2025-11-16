#pragma once

#include "CompletedGesture.hpp"
#include "DragGesture.hpp"
#include "Logger.hpp"
#include "Shared.hpp"
#include <memory>
#include <optional>
#include <wayfire/touch/touch.hpp>

struct SMonitorArea {
    double x, y, w, h;
};

/*
 * Interface; there's only @CGestures and the mock gesture manager for testing
 * that implements this
 *
 * New gesture_t are added with @addTouchGesture(). Callbacks are triggered
 * during updateGestures if all actions within a geture_t is completed (actions
 * are chained serially, i.e. one action must be "completed" before the next
 * can start "running")
 */
class IGestureManager {
  public:
    IGestureManager(std::unique_ptr<Logger> logger) : logger(std::move(logger)) {}
    virtual ~IGestureManager() {}
    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchDown(const wf::touch::gesture_event_t&);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchUp(const wf::touch::gesture_event_t&);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchMove(const wf::touch::gesture_event_t&);

    void addTouchGesture(std::unique_ptr<wf::touch::gesture_t> gesture);
    void addMultiFingerGesture(
        double base_threshold, double base_finger_slip, const float* sensitivity, const int64_t* timeout
    );
    void addMultiFingerTap(double base_finger_slip, const float* sensitivity, const int64_t* timeout);
    void addLongPress(double base_finger_slip, const float* sensitivity, const int64_t* delay);
    void addEdgeSwipeGesture(
        double base_threshold, double base_finger_slip, const float* sensitivity, const int64_t* timeout,
        const long* edge_margin
    );
    void addPinchGesture(double base_threshold, const float* sensitivity, const int64_t* timeout);

    std::optional<DragGestureEvent> getActiveDragGesture() const {
        return activeDragGesture;
    }

    // indicates whether events should be blocked from forwarding to client
    // windows/surfaces
    bool eventForwardingInhibited() const {
        return inhibitTouchEvents;
    };

  protected:
    std::vector<std::unique_ptr<wf::touch::gesture_t>> m_vGestures;
    wf::touch::gesture_state_t m_sGestureState;

    GestureDirection find_swipe_edges(wf::touch::point_t point, int edge_margin);
    virtual SMonitorArea getMonitorArea() const = 0;

    // checks if the gesture event has a corresponding handler. longpress skips this
    // and calls handleCompletedGesture directly because it's CompletedGestureEvent
    // is emitted at drag begin.
    virtual bool findCompletedGesture(const CompletedGestureEvent& gev) const = 0;
    // handles gesture events and returns whether or not the event is used.
    virtual bool handleCompletedGesture(const CompletedGestureEvent& gev) = 0;

    // called at the start of drag evetns and returns whether or not the event is used.
    virtual bool handleDragGesture(const DragGestureEvent& gev) = 0;

    // called on every touch event while a drag gesture is active
    virtual void dragGestureUpdate(const wf::touch::gesture_event_t&) = 0;

    // called at the end of a drag event
    virtual void handleDragGestureEnd(const DragGestureEvent& gev) = 0;

    // this function should cleanup after drag gestures
    virtual void handleCancelledGesture() = 0;

    virtual void updateLongPressTimer(uint32_t current_time, uint32_t delay) = 0;
    virtual void stopLongPressTimer()                                        = 0;

    virtual void debugLog(const std::string& msg) {};

  private:
    std::unique_ptr<Logger> logger;
    bool inhibitTouchEvents;
    bool gestureTriggered; // A drag/completed gesture is triggered
    std::optional<DragGestureEvent> activeDragGesture;
    std::optional<CompletedGestureEvent> promisedCompletedGesture;

    // this function is called when needed to send "cancel touch" events to
    // client windows/surfaces
    virtual void sendCancelEventsToWindows() = 0;

    // try to find a CompletedGesture to later emit
    bool reserveCompletedGesture(const CompletedGestureEvent& gev);
    bool emitCompletedGesture(const CompletedGestureEvent& gev);
    bool emitDragGesture(const DragGestureEvent& gev);
    bool emitDragGestureEnd(const DragGestureEvent& gev);

    void updateGestures(const wf::touch::gesture_event_t&);
    void cancelTouchEventsOnAllWindows();
};
