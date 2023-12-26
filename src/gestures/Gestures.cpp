#include "Gestures.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <wayfire/touch/touch.hpp>

std::string stringifyDirection(gestureDirection direction) {
    std::string bind;
    if (direction & GESTURE_DIRECTION_LEFT) {
        bind += 'l';
    }

    if (direction & GESTURE_DIRECTION_RIGHT) {
        bind += 'r';
    }

    if (direction & GESTURE_DIRECTION_UP) {
        bind += 'u';
    }

    if (direction & GESTURE_DIRECTION_DOWN) {
        bind += 'd';
    }

    return bind;
}

std::string CompletedGesture::to_string() const {
    switch (type) {
        case CompletedGestureType::EDGE_SWIPE:
            return "edge:" + stringifyDirection(this->edge_origin) + ":" + stringifyDirection(this->direction);
        case CompletedGestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
            break;
        case CompletedGestureType::TAP:
            return "tap:" + std::to_string(finger_count);
        case CompletedGestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
    }

    return "";
}

std::string DragGesture::to_string() const {
    switch (type) {
        case DragGestureType::LONG_PRESS:
            return "longpress:" + std::to_string(finger_count);
        case DragGestureType::SWIPE:
            return "swipe:" + std::to_string(finger_count) + ":" + stringifyDirection(this->direction);
    }

    return "";
}

wf::touch::action_status_t CMultiAction::update_state(const wf::touch::gesture_state_t& state,
                                                      const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->timeout) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        // cancel if previous fingers moved too much
        this->finger_count = state.fingers.size();
        for (auto& finger : state.fingers) {
            // TODO multiply tolerance by sensitivity?
            if (glm::length(finger.second.delta()) > GESTURE_INITIAL_TOLERANCE) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }

        return wf::touch::ACTION_STATUS_RUNNING;
    }

    if ((glm::length(state.get_center().delta()) >= MIN_SWIPE_DISTANCE) && (this->target_direction == 0)) {
        this->target_direction = state.get_center().get_direction();
    }

    if (this->target_direction == 0) {
        return wf::touch::ACTION_STATUS_RUNNING;
    }

    for (auto& finger : state.fingers) {
        if (finger.second.get_incorrect_drag_distance(this->target_direction) > this->get_move_tolerance()) {
            return wf::touch::ACTION_STATUS_CANCELLED;
        }
    }

    if (state.get_center().get_drag_distance(target_direction) >= base_threshold / *sensitivity) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }
    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t MultiFingerDownAction::update_state(const wf::touch::gesture_state_t& state,
                                                               const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN && state.fingers.size() >= SEND_CANCEL_EVENT_FINGER_COUNT) {
        this->callback();
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t MultiFingerTap::update_state(const wf::touch::gesture_state_t& state,
                                                        const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->timeout) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    if (event.type == wf::touch::EVENT_TYPE_MOTION) {
        for (const auto& finger : state.fingers) {
            const auto delta = finger.second.delta();
            if (delta.x * delta.x + delta.y + delta.y > this->base_threshold / *this->sensitivity) {
                return wf::touch::ACTION_STATUS_CANCELLED;
            }
        }
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t LongPress::update_state(const wf::touch::gesture_state_t& state,
                                                   const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > *this->delay) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    switch (event.type) {
        case wf::touch::EVENT_TYPE_MOTION:
            for (const auto& finger : state.fingers) {
                const auto delta = finger.second.delta();
                if (delta.x * delta.x + delta.y + delta.y > this->base_threshold / *this->sensitivity) {
                    return wf::touch::ACTION_STATUS_CANCELLED;
                }
            }
            break;

        case wf::touch::EVENT_TYPE_TOUCH_DOWN:
            // TODO: also reset wl_timer here
            gesture_action_t::reset(event.time);
            this->update_external_timer_callback(event.time, *this->delay);
            break;

        case wf::touch::EVENT_TYPE_TOUCH_UP:
            return wf::touch::ACTION_STATUS_CANCELLED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t LiftoffAction::update_state(const wf::touch::gesture_state_t& state,
                                                       const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t TouchUpOrDownAction::update_state(const wf::touch::gesture_state_t& state,
                                                             const wf::touch::gesture_event_t& event) {
    if (event.time - this->start_time > this->get_duration()) {
        return wf::touch::ACTION_STATUS_CANCELLED;
    }

    if (event.type == wf::touch::EVENT_TYPE_TOUCH_UP || event.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        return wf::touch::ACTION_STATUS_COMPLETED;
    }

    return wf::touch::ACTION_STATUS_RUNNING;
}

wf::touch::action_status_t OnCompleteAction::update_state(const wf::touch::gesture_state_t& state,
                                                          const wf::touch::gesture_event_t& event) {
    auto status = this->action->update_state(state, event);

    if (status == wf::touch::ACTION_STATUS_COMPLETED) {
        this->callback();
    }

    return status;
}

void IGestureManager::updateGestures(const wf::touch::gesture_event_t& ev) {
    if (m_sGestureState.fingers.size() == 1 && ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        this->inhibitTouchEvents = false;
        this->activeDragGesture  = std::nullopt;
    }
    for (const auto& gesture : m_vGestures) {
        if (m_sGestureState.fingers.size() == 1 && ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
            gesture->reset(ev.time);
        }

        gesture->update_state(ev);
    }
}

void IGestureManager::cancelTouchEventsOnAllWindows() {
    if (!this->inhibitTouchEvents) {
        this->inhibitTouchEvents = true;
        this->sendCancelEventsToWindows();
    }
}

// @return whether or not to inhibit further actions
bool IGestureManager::onTouchDown(const wf::touch::gesture_event_t& ev) {
    // NOTE @m_sGestureState is used in gesture-completed callbacks
    // during touch down it must be updated before updating the gestures
    // in touch up and motion, it must be updated AFTER updating the
    // gestures
    this->m_sGestureState.update(ev);
    this->updateGestures(ev);

    if (this->activeDragGesture.has_value()) {
        this->dragGestureUpdate(ev);
    }

    return this->eventForwardingInhibited();
}

bool IGestureManager::onTouchUp(const wf::touch::gesture_event_t& ev) {
    this->updateGestures(ev);
    this->m_sGestureState.update(ev);

    if (this->activeDragGesture.has_value()) {
        this->dragGestureUpdate(ev);
    }

    return this->eventForwardingInhibited();
}

bool IGestureManager::onTouchMove(const wf::touch::gesture_event_t& ev) {
    this->updateGestures(ev);
    this->m_sGestureState.update(ev);

    if (this->activeDragGesture.has_value()) {
        this->dragGestureUpdate(ev);
    }

    return this->eventForwardingInhibited();
}

gestureDirection IGestureManager::find_swipe_edges(wf::touch::point_t point) {
    auto mon = this->getMonitorArea();

    gestureDirection edge_directions = 0;

    if (point.x <= mon.x + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_LEFT;
    }

    if (point.x >= mon.x + mon.w - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_RIGHT;
    }

    if (point.y <= mon.y + EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_UP;
    }

    if (point.y >= mon.y + mon.h - EDGE_SWIPE_THRESHOLD) {
        edge_directions |= GESTURE_DIRECTION_DOWN;
    }

    return edge_directions;
}

void IGestureManager::addTouchGesture(std::unique_ptr<wf::touch::gesture_t> gesture) {
    this->m_vGestures.emplace_back(std::move(gesture));
}

// Adds a Multi-fingered swipe:
// * inhibits events to client windows/surfaces when enough fingers touch
//   down within a short duration.
// * emits a TouchGestureType::SWIPE_HOLD event once fingers moved over the
//   threshold.
// * further emits a TouchGestureType::SWIPE event if the SWIPE_HOLD event was
//   emitted and once a finger is lifted
//   TODO: ^^^ remove
void IGestureManager::addMultiFingerGesture(const float* sensitivity, const int64_t* timeout) {
    auto multi_down = std::make_unique<MultiFingerDownAction>([this]() { this->cancelTouchEventsOnAllWindows(); });
    multi_down->set_duration(GESTURE_BASE_DURATION);

    auto swipe = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);

    auto swipe_ptr = swipe.get();

    auto swipe_and_emit = std::make_unique<OnCompleteAction>(std::move(swipe), [=, this]() {
        if (this->activeDragGesture.has_value()) {
            return;
        }
        const auto gesture = DragGesture{DragGestureType::SWIPE, swipe_ptr->target_direction,
                                         static_cast<int>(this->m_sGestureState.fingers.size())};

        this->activeDragGesture = this->handleDragGesture(gesture) ? std::optional(gesture) : std::nullopt;
    });

    auto swipe_liftoff = std::make_unique<LiftoffAction>();
    // swipe_liftoff->set_duration(GESTURE_BASE_DURATION / 2);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(multi_down));
    swipe_actions.emplace_back(std::move(swipe_and_emit));
    swipe_actions.emplace_back(std::move(swipe_liftoff));

    auto ack = [swipe_ptr, this]() {
        if (this->activeDragGesture.has_value() && this->activeDragGesture->type == DragGestureType::SWIPE) {
            const auto gesture =
                DragGesture{DragGestureType::SWIPE, 0, static_cast<int>(this->m_sGestureState.fingers.size())};

            this->handleDragGestureEnd(gesture);
            this->activeDragGesture = std::nullopt;
        } else {
            const auto gesture = CompletedGesture{CompletedGestureType::SWIPE, swipe_ptr->target_direction,
                                                  static_cast<int>(this->m_sGestureState.fingers.size())};

            this->handleCompletedGesture(gesture);
        }
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(swipe_actions), ack, cancel));
}

void IGestureManager::addMultiFingerTap(const float* sensitivity, const int64_t* timeout) {
    auto tap = std::make_unique<MultiFingerTap>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> tap_actions;
    tap_actions.emplace_back(std::move(tap));

    auto ack = [this]() {
        const auto gesture =
            CompletedGesture{CompletedGestureType::TAP, 0, static_cast<int>(this->m_sGestureState.fingers.size())};
        this->handleCompletedGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(tap_actions), ack, cancel));
}

void IGestureManager::addLongPress(const float* sensitivity, const int64_t* delay) {
    auto long_press_and_emit = std::make_unique<OnCompleteAction>(
        std::make_unique<LongPress>(
            SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, delay,
            [this](uint32_t current_time, uint32_t delay) { this->updateLongPressTimer(current_time, delay); }),
        [this]() {
            if (this->activeDragGesture.has_value()) {
                return;
            }
            const auto gesture =
                DragGesture{DragGestureType::LONG_PRESS, 0, static_cast<int>(this->m_sGestureState.fingers.size())};

            this->activeDragGesture = this->handleDragGesture(gesture) ? std::optional(gesture) : std::nullopt;
        });

    auto touch_up_or_down = std::make_unique<TouchUpOrDownAction>();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> long_press_actions;
    long_press_actions.emplace_back(std::move(long_press_and_emit));
    long_press_actions.emplace_back(std::move(touch_up_or_down));

    auto ack = [this]() {
        if (this->activeDragGesture.has_value() && this->activeDragGesture->type == DragGestureType::LONG_PRESS) {
            const auto gesture =
                DragGesture{DragGestureType::LONG_PRESS, 0, static_cast<int>(this->m_sGestureState.fingers.size())};

            this->handleDragGestureEnd(gesture);
            this->activeDragGesture = std::nullopt;
        } else {
            const auto gesture = CompletedGesture{CompletedGestureType::LONG_PRESS, 0,
                                                  static_cast<int>(this->m_sGestureState.fingers.size())};

            this->handleCompletedGesture(gesture);
        };
    };
    auto cancel = [this]() {
        this->stopLongPressTimer();
        this->handleCancelledGesture();
    };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(long_press_actions), ack, cancel));
}

void IGestureManager::addEdgeSwipeGesture(const float* sensitivity, const int64_t* timeout) {
    // Edge swipe needs a quick release to be considered edge swipe
    auto edge         = std::make_unique<CMultiAction>(MAX_SWIPE_DISTANCE, sensitivity, timeout);
    auto edge_release = std::make_unique<wf::touch::touch_action_t>(1, false);

    // TODO do I really need this:
    // edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * *sensitivity);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    // TODO make this adjustable:
    edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * *sensitivity);

    auto edge_ptr = edge.get();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> edge_swipe_actions;
    edge_swipe_actions.emplace_back(std::move(edge));
    edge_swipe_actions.emplace_back(std::move(edge_release));

    auto ack = [edge_ptr, this]() {
        auto origin_edges = find_swipe_edges(m_sGestureState.get_center().origin);

        if (!origin_edges) {
            return;
        }
        auto direction = edge_ptr->target_direction;
        auto gesture =
            CompletedGesture{CompletedGestureType::EDGE_SWIPE, direction, edge_ptr->finger_count, origin_edges};
        this->handleCompletedGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    auto gesture = std::make_unique<wf::touch::gesture_t>(std::move(edge_swipe_actions), ack, cancel);
    this->addTouchGesture(std::move(gesture));
}
