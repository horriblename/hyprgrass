#include "Shared.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <wayfire/touch/touch.hpp>

using UpdateExternalTimerCallback = std::function<void(uint32_t current_timer, uint32_t delay)>;

// swipe and with multiple fingers and directions
class CMultiAction : public wf::touch::gesture_action_t {
  private:
    double base_threshold;
    // How much *each* finger is allowed to travel in the wrong direction.
    // Should be smaller than base_threshold.
    double base_finger_slip;
    const float* sensitivity;
    const int64_t* timeout;

  public:
    //   threshold = base_threshold / sensitivity
    // if the threshold needs to be adjusted dynamically, the sensitivity
    // pointer is used
    CMultiAction(double base_threshold, double base_finger_slip, const float* sensitivity, const int64_t* timeout)
        : base_threshold(base_threshold), base_finger_slip(base_finger_slip), sensitivity(sensitivity),
          timeout(timeout) {};

    GestureDirection target_direction = 0;
    int finger_count                  = 0;

    // The action is completed if any number of fingers is moved enough.
    //
    // This action should be followed by another that completes upon lifting a
    // finger to achieve a gesture that completes after a multi-finger swipe is
    // done and lifted.
    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;

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
        : base_threshold(base_threshold), sensitivity(sensitivity), timeout(timeout) {};

    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;
};

class LongPress : public wf::touch::gesture_action_t {
  private:
    double base_threshold;
    const float* sensitivity;
    const int64_t* delay;
    UpdateExternalTimerCallback update_external_timer_callback;

  public:
    // TODO: I hope one day I can figure out how not to pass a function for the update timer callback
    LongPress(
        double base_threshold, const float* sensitivity, const int64_t* delay,
        UpdateExternalTimerCallback update_external_timer
    )
        : base_threshold(base_threshold), sensitivity(sensitivity), delay(delay),
          update_external_timer_callback(update_external_timer) {};

    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;
};

// Completes upon receiving a touch up event and cancels upon receiving a touch
// down event.
class LiftoffAction : public wf::touch::gesture_action_t {
    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;
};

// Completes upon receiving a touch up or touch down event
class TouchUpOrDownAction : public wf::touch::gesture_action_t {
    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;
};

// Completes upon all touch points lifted.
class LiftAll : public wf::touch::gesture_action_t {
    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;
};

// This action is used to call a function right after another action is completed
class OnCompleteAction : public wf::touch::gesture_action_t {
    using Callback = std::function<void(uint32_t)>;

  private:
    std::unique_ptr<wf::touch::gesture_action_t> action;
    const Callback callback;

  public:
    OnCompleteAction(std::unique_ptr<wf::touch::gesture_action_t> action, Callback callback) : callback(callback) {
        this->action = std::move(action);
    }

    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;

    void reset(uint32_t time) override {
        this->action->reset(time);
    }
};

class PinchAction : public wf::touch::gesture_action_t {
  public:
    /**
     * Create a new pinch action.
     *
     * @param threshold The threshold to be exceeded.
     * Example: if the threshold is 0.2,
     * - you need to pinch in to 80% of the initial size (1.0 - 0.2 = 0.8)
     * - you need to pinch out to 120% of the initial size (1.0 + 0.2)
     */
    PinchAction(float base_threshold, const float* sensitivity)
        : base_threshold(base_threshold), sensitivity(sensitivity) {}

    /**
     * The action is already completed iff no fingers have been added or
     * released and the pinch threshold has been reached without much movement.
     */
    wf::touch::action_status_t
    update_state(const wf::touch::gesture_state_t& state, const wf::touch::gesture_event_t& event) override;

  protected:
    /**
     * @return True if gesture center has moved more than tolerance.
     */
    bool exceeds_tolerance(const wf::touch::gesture_state_t& state) override;

  private:
    const float base_threshold;
    const float* sensitivity;
    std::optional<float> initial_span = std::nullopt;
    uint32_t move_tolerance           = 1e9;
};
