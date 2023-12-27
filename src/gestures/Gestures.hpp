#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <wayfire/touch/touch.hpp>

// Swipe params
constexpr static int EDGE_SWIPE_THRESHOLD              = 10;
constexpr static double MIN_SWIPE_DISTANCE             = 30;
constexpr static double MAX_SWIPE_DISTANCE             = 450;
constexpr static double SWIPE_INCORRECT_DRAG_TOLERANCE = 150;

// Pinch params
constexpr static double PINCH_INCORRECT_DRAG_TOLERANCE = 200;
constexpr static double PINCH_THRESHOLD                = 1.5;

// Hold params
constexpr static double HOLD_INCORRECT_DRAG_TOLERANCE = 100;

// General
constexpr static double GESTURE_INITIAL_TOLERANCE = 40;
constexpr static uint32_t GESTURE_BASE_DURATION   = 400;

constexpr static uint32_t SEND_CANCEL_EVENT_FINGER_COUNT = 3;

using UpdateExternalTimerCallback = std::function<void(uint32_t current_timer, uint32_t delay)>;

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
};

enum TouchGestureDirection {
    /* Swipe-specific */
    GESTURE_DIRECTION_LEFT  = (1 << 0),
    GESTURE_DIRECTION_RIGHT = (1 << 1),
    GESTURE_DIRECTION_UP    = (1 << 2),
    GESTURE_DIRECTION_DOWN  = (1 << 3),
    /* Pinch-specific */
    // GESTURE_DIRECTION_IN = (1 << 4),
    // GESTURE_DIRECTION_OUT = (1 << 5),
};

// can be one of @eTouchGestureDirection or a combination of them
using gestureDirection = uint32_t;

/**
 * Represents a touch gesture.
 *
 * Finger count can be arbitrary (might be a good idea to limit to >3)
 */
struct CompletedGesture {
    CompletedGestureType type;
    gestureDirection direction;
    int finger_count;

    // TODO turn this whole struct into a sum type?
    // edge swipe specific
    gestureDirection edge_origin;

    std::string to_string() const;
};

struct DragGesture {
    DragGestureType type;
    gestureDirection direction;
    int finger_count;

    std::string to_string() const;
};

struct SMonitorArea {
    double x, y, w, h;
};

// swipe and with multiple fingers and directions
class CMultiAction : public wf::touch::gesture_action_t {
  private:
    double base_threshold;
    const float* sensitivity;
    const int64_t* timeout;

  public:
    //   threshold = base_threshold / sensitivity
    // if the threshold needs to be adjusted dynamically, the sensitivity
    // pointer is used
    CMultiAction(double base_threshold, const float* sensitivity, const int64_t* timeout)
        : base_threshold(base_threshold), sensitivity(sensitivity), timeout(timeout){};

    gestureDirection target_direction = 0;
    int finger_count                  = 0;

    // The action is completed if any number of fingers is moved enough.
    //
    // This action should be followed by another that completes upon lifting a
    // finger to achieve a gesture that completes after a multi-finger swipe is
    // done and lifted.
    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;

    void reset(uint32_t time) override {
        gesture_action_t::reset(time);
        target_direction = 0;
    };
};

class MultiFingerTap : public wf::touch::gesture_action_t {
  private:
    double base_threshold;
    const float* sensitivity;
    const int64_t* timeout;

  public:
    MultiFingerTap(double base_threshold, const float* sensitivity, const int64_t* timeout)
        : base_threshold(base_threshold), sensitivity(sensitivity), timeout(timeout){};

    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;
};

class LongPress : public wf::touch::gesture_action_t {
  private:
    double base_threshold;
    const float* sensitivity;
    const int64_t* delay;
    UpdateExternalTimerCallback update_external_timer_callback;

  public:
    // TODO: I hope one day I can figure out how not to pass a function for the update timer callback
    LongPress(double base_threshold, const float* sensitivity, const int64_t* delay,
              UpdateExternalTimerCallback update_external_timer)
        : base_threshold(base_threshold), sensitivity(sensitivity), delay(delay),
          update_external_timer_callback(update_external_timer){};

    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;
};

// Completes upon receiving enough touch down events within a short duration
class MultiFingerDownAction : public wf::touch::gesture_action_t {
    // upon completion, calls the given callback.
    //
    // Intended to be used to send cancel events to surfaces when enough fingers
    // touch down in quick succession.
  public:
    MultiFingerDownAction() {}

    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;
};

// Completes upon receiving a touch up event and cancels upon receiving a touch
// down event.
class LiftoffAction : public wf::touch::gesture_action_t {
    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;
};

// Completes upon receiving a touch up or touch down event
class TouchUpOrDownAction : public wf::touch::gesture_action_t {
    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;
};

// This action is used to call a function right after another action is completed
class OnCompleteAction : public wf::touch::gesture_action_t {
  private:
    std::unique_ptr<wf::touch::gesture_action_t> action;
    const std::function<void()> callback;

  public:
    OnCompleteAction(std::unique_ptr<wf::touch::gesture_action_t> action, std::function<void()> callback)
        : callback(callback) {
        this->action = std::move(action);
    }

    wf::touch::action_status_t update_state(const wf::touch::gesture_state_t& state,
                                            const wf::touch::gesture_event_t& event) override;

    void reset(uint32_t time) override {
        this->action->reset(time);
    }
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

    gestureDirection find_swipe_edges(wf::touch::point_t point);
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

    void updateGestures(const wf::touch::gesture_event_t&);
    void cancelTouchEventsOnAllWindows();
};
