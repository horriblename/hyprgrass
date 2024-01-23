#pragma once
#include "Shared.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <wayfire/touch/touch.hpp>

enum class CompletedGestureType {
    // Invalid Gesture
    SWIPE,
    EDGE_SWIPE,
    TAP,
    LONG_PRESS,
    // PINCH,
};

enum class DragGestureType {
    SWIPE,
    LONG_PRESS,
    EDGE_SWIPE,
};

/**
 * Represents a touch gesture.
 *
 * Finger count can be arbitrary (might be a good idea to limit to >3)
 */
struct CompletedGesture {
    CompletedGestureType type;
    GestureDirection direction;
    int finger_count;

    // TODO turn this whole struct into a sum type?
    // edge swipe specific
    GestureDirection edge_origin;

    std::string to_string() const;
};

struct DragGesture {
    DragGestureType type;
    GestureDirection direction;
    int finger_count;

    GestureDirection edge_origin;

    std::string to_string() const;
};

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
    void addMultiFingerGesture(const float* sensitivity, const int64_t* timeout);
    void addMultiFingerTap(const float* sensitivity, const int64_t* timeout);
    void addLongPress(const float* sensitivity, const int64_t* delay);
    void addEdgeSwipeGesture(const float* sensitivity, const int64_t* timeout);

    std::optional<DragGesture> getActiveDragGesture() const {
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

    GestureDirection find_swipe_edges(wf::touch::point_t point);
    virtual SMonitorArea getMonitorArea() const = 0;

    // handles gesture events and returns whether or not the event is used.
    virtual bool handleCompletedGesture(const CompletedGesture& gev) = 0;

    // called at the start of drag evetns and returns whether or not the event is used.
    virtual bool handleDragGesture(const DragGesture& gev) = 0;

    // called on every touch event while a drag gesture is active
    virtual void dragGestureUpdate(const wf::touch::gesture_event_t&) = 0;

    // called at the end of a drag event
    virtual void handleDragGestureEnd(const DragGesture& gev) = 0;

    // this function should cleanup after drag gestures
    virtual void handleCancelledGesture() = 0;

    virtual void updateLongPressTimer(uint32_t current_time, uint32_t delay) = 0;
    virtual void stopLongPressTimer()                                        = 0;

  private:
    bool inhibitTouchEvents;
    std::optional<DragGesture> activeDragGesture;

    // this function is called when needed to send "cancel touch" events to
    // client windows/surfaces
    virtual void sendCancelEventsToWindows() = 0;

    bool emitCompletedGesture(const CompletedGesture& gev);
    bool emitDragGesture(const DragGesture& gev);

    void updateGestures(const wf::touch::gesture_event_t&);
    void cancelTouchEventsOnAllWindows();
};
