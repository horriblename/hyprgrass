#include "Gestures.hpp"
#include "Actions.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <wayfire/touch/touch.hpp>

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
bool IGestureManager::emitCompletedGesture(const CompletedGesture& gev) {
    bool handled = this->handleCompletedGesture(gev);
    if (handled) {
        this->stopLongPressTimer();
    }

    return handled;
}

bool IGestureManager::emitDragGesture(const DragGesture& gev) {
    bool handled = this->handleDragGesture(gev);
    if (handled) {
        this->stopLongPressTimer();
    }

    return handled;
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

GestureDirection IGestureManager::find_swipe_edges(wf::touch::point_t point) {
    auto mon = this->getMonitorArea();

    GestureDirection edge_directions = 0;

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

void IGestureManager::addMultiFingerGesture(const float* sensitivity, const int64_t* timeout) {
    auto multi_down_and_send_cancel = std::make_unique<OnCompleteAction>(
        std::make_unique<MultiFingerDownAction>(), [this]() { this->cancelTouchEventsOnAllWindows(); });
    multi_down_and_send_cancel->set_duration(GESTURE_BASE_DURATION);

    auto swipe = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);

    auto swipe_ptr = swipe.get();

    auto swipe_and_emit = std::make_unique<OnCompleteAction>(std::move(swipe), [=, this]() {
        if (this->activeDragGesture.has_value()) {
            return;
        }
        const auto gesture = DragGesture{DragGestureType::SWIPE, swipe_ptr->target_direction,
                                         static_cast<int>(this->m_sGestureState.fingers.size())};

        this->activeDragGesture = this->emitDragGesture(gesture) ? std::optional(gesture) : std::nullopt;
    });

    auto swipe_liftoff = std::make_unique<LiftoffAction>();
    // swipe_liftoff->set_duration(GESTURE_BASE_DURATION / 2);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(multi_down_and_send_cancel));
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

            this->emitCompletedGesture(gesture);
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
        this->emitCompletedGesture(gesture);
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

            this->activeDragGesture = this->emitDragGesture(gesture) ? std::optional(gesture) : std::nullopt;
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

            this->emitCompletedGesture(gesture);
        };
    };
    auto cancel = [this]() {
        this->stopLongPressTimer();
        this->handleCancelledGesture();
    };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(long_press_actions), ack, cancel));
}

void IGestureManager::addEdgeSwipeGesture(const float* sensitivity, const int64_t* timeout) {
    auto edge            = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);
    auto edge_ptr        = edge.get();
    auto edge_drag_begin = std::make_unique<OnCompleteAction>(std::move(edge), [=, this]() {
        auto origin_edges = this->find_swipe_edges(m_sGestureState.get_center().origin);

        if (origin_edges == 0) {
            return;
        }
        auto direction = edge_ptr->target_direction;
        auto gesture   = DragGesture{DragGestureType::EDGE_SWIPE, direction, edge_ptr->finger_count, origin_edges};
        this->activeDragGesture = this->emitDragGesture(gesture) ? std::optional(gesture) : std::nullopt;
    });
    auto edge_release    = std::make_unique<wf::touch::touch_action_t>(1, false);

    // TODO do I really need this:
    // edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * *sensitivity);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    // TODO make this adjustable:
    edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * *sensitivity);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> edge_swipe_actions;
    edge_swipe_actions.emplace_back(std::move(edge_drag_begin));
    edge_swipe_actions.emplace_back(std::move(edge_release));

    auto ack = [edge_ptr, this]() {
        auto origin_edges = find_swipe_edges(m_sGestureState.get_center().origin);

        if (origin_edges == 0) {
            return;
        }
        auto direction = edge_ptr->target_direction;
        auto gesture =
            CompletedGesture{CompletedGestureType::EDGE_SWIPE, direction, edge_ptr->finger_count, origin_edges};
        this->emitCompletedGesture(gesture);
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    auto gesture = std::make_unique<wf::touch::gesture_t>(std::move(edge_swipe_actions), ack, cancel);
    this->addTouchGesture(std::move(gesture));
}
