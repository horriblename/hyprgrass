#include "Gestures.hpp"
#include "Actions.hpp"
#include "CompletedGesture.hpp"
#include "DragGesture.hpp"
#include "Shared.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <wayfire/touch/touch.hpp>

void IGestureManager::updateGestures(const wf::touch::gesture_event_t& ev) {
    if (m_sGestureState.fingers.size() == 1 && ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN) {
        this->inhibitTouchEvents = false;
        this->activeDragGesture  = std::nullopt;
    }
    bool should_reset      = m_sGestureState.fingers.size() == 1 && ev.type == wf::touch::EVENT_TYPE_TOUCH_DOWN;
    this->gestureTriggered = false;
    for (const auto& gesture : m_vGestures) {
        if (should_reset) {
            gesture->reset(ev.time);
        }

        if (!this->gestureTriggered) {
            gesture->update_state(ev);
        }
    }
}

void IGestureManager::cancelTouchEventsOnAllWindows() {
    if (!this->inhibitTouchEvents) {
        this->inhibitTouchEvents = true;
        this->sendCancelEventsToWindows();
    }
}
bool IGestureManager::emitCompletedGesture(const CompletedGestureEvent& gev) {
    bool handled = this->handleCompletedGesture(gev);
    if (handled) {
        // FIXME: I'm trying to prevent swipe:1:r from triggering when edge:l:r triggers
        // this only prevents it when there is a valid edge swipe, is that fine?
        this->gestureTriggered = true;
        this->stopLongPressTimer();
    }

    return handled;
}

bool IGestureManager::emitDragGesture(const DragGestureEvent& gev) {
    bool handled = this->handleDragGesture(gev);
    if (handled) {
        this->gestureTriggered  = true;
        this->activeDragGesture = std::optional(gev);
        this->stopLongPressTimer();
    }

    return handled;
}

bool IGestureManager::emitDragGestureEnd(const DragGestureEvent& gev) {
    if (this->activeDragGesture.has_value() && this->activeDragGesture->type == gev.type) {

        this->handleDragGestureEnd(gev);
        this->activeDragGesture = std::nullopt;
        return true;
    }
    return false;
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

GestureDirection IGestureManager::find_swipe_edges(wf::touch::point_t point, int edge_margin) {
    auto mon = this->getMonitorArea();

    GestureDirection edge_directions = 0;

    if (point.x <= mon.x + edge_margin) {
        edge_directions |= GESTURE_DIRECTION_LEFT;
    }

    if (point.x >= mon.x + mon.w - edge_margin) {
        edge_directions |= GESTURE_DIRECTION_RIGHT;
    }

    if (point.y <= mon.y + edge_margin) {
        edge_directions |= GESTURE_DIRECTION_UP;
    }

    if (point.y >= mon.y + mon.h - edge_margin) {
        edge_directions |= GESTURE_DIRECTION_DOWN;
    }

    return edge_directions;
}

void IGestureManager::addTouchGesture(std::unique_ptr<wf::touch::gesture_t> gesture) {
    this->m_vGestures.emplace_back(std::move(gesture));
}

void IGestureManager::addMultiFingerGesture(
    const float* sensitivity, const int64_t* timeout, const float* pinch_threshold
) {
    auto multi_down_and_send_cancel =
        std::make_unique<OnCompleteAction>(std::make_unique<MultiFingerDownAction>(), [this](uint32_t) {
            this->cancelTouchEventsOnAllWindows();
        });
    multi_down_and_send_cancel->set_duration(GESTURE_BASE_DURATION);

    auto swipe = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);

    auto swipe_ptr = swipe.get();

    auto swipe_and_emit = std::make_unique<OnCompleteAction>(std::move(swipe), [=, this](uint32_t time) {
        if (this->activeDragGesture.has_value()) {
            return;
        }

        const auto gesture = DragGestureEvent{
            .time         = time,
            .type         = DragGestureType::SWIPE,
            .direction    = swipe_ptr->target_direction,
            .finger_count = static_cast<uint32_t>(this->m_sGestureState.fingers.size())
        };

        if (this->emitDragGesture(gesture)) {
            this->cancelTouchEventsOnAllWindows();
        }
    });

    auto swipe_liftoff   = std::make_unique<LiftoffAction>();
    auto liftoff_and_ack = std::make_unique<OnCompleteAction>(std::move(swipe_liftoff), [=, this](uint32_t time) {
        const auto drag = DragGestureEvent{
            .time         = time,
            .type         = DragGestureType::SWIPE,
            .direction    = 0,
            .finger_count = static_cast<uint32_t>(this->m_sGestureState.fingers.size())
        };
        if (this->emitDragGestureEnd(drag)) {
            return;
        } else {
            double pinch_scale = this->m_sGestureState.get_pinch_scale();
            double lo          = std::clamp(1.0 - *pinch_threshold, 0.1, 1.0);
            double hi          = 1.0 + *pinch_threshold;
            hi                 = hi < 1.0 ? 1.0 : hi;
            if (pinch_scale < lo || pinch_scale > hi)
                return;

            const auto gesture = CompletedGestureEvent{
                CompletedGestureType::SWIPE, swipe_ptr->target_direction,
                static_cast<int>(this->m_sGestureState.fingers.size())
            };

            this->emitCompletedGesture(gesture);
        }
    });

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> swipe_actions;
    swipe_actions.emplace_back(std::move(swipe_and_emit));
    swipe_actions.emplace_back(std::move(liftoff_and_ack));

    auto cancel = [this]() { this->handleCancelledGesture(); };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(swipe_actions), []() {}, cancel));
}

void IGestureManager::addMultiFingerTap(const float* sensitivity, const int64_t* timeout) {
    auto tap = std::make_unique<MultiFingerTap>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> tap_actions;
    tap_actions.emplace_back(std::move(tap));

    auto ack = [this]() {
        const auto gesture =
            CompletedGestureEvent{CompletedGestureType::TAP, 0, static_cast<int>(this->m_sGestureState.fingers.size())};
        if (this->emitCompletedGesture(gesture)) {
            this->cancelTouchEventsOnAllWindows();
        }
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(tap_actions), ack, cancel));
}

void IGestureManager::addLongPress(const float* sensitivity, const int64_t* delay) {
    auto long_press_and_emit = std::make_unique<OnCompleteAction>(
        std::make_unique<LongPress>(
            SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, delay,
            [this](uint32_t current_time, uint32_t delay) { this->updateLongPressTimer(current_time, delay); }
        ),
        [this](uint32_t time) {
            if (this->activeDragGesture.has_value()) {
                return;
            }
            const auto gesture = DragGestureEvent{
                .time         = time,
                .type         = DragGestureType::LONG_PRESS,
                .direction    = 0,
                .finger_count = static_cast<uint32_t>(this->m_sGestureState.fingers.size())
            };

            const auto gesture1 = CompletedGestureEvent{
                CompletedGestureType::LONG_PRESS, 0, static_cast<int>(this->m_sGestureState.fingers.size())
            };

            bool handled = this->emitDragGesture(gesture) || this->emitCompletedGesture(gesture1);

            if (handled) {
                this->cancelTouchEventsOnAllWindows();
            }
        }
    );

    auto lift_all_and_ack = std::make_unique<OnCompleteAction>(std::make_unique<LiftAll>(), [this](uint32_t time) {
        if (this->activeDragGesture.has_value()) {
            DragGestureEvent gev = {this->activeDragGesture.value()};
            gev.time             = time;
            this->emitDragGestureEnd(gev);
            return;
        };
    });

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> long_press_actions;
    long_press_actions.emplace_back(std::move(long_press_and_emit));
    long_press_actions.emplace_back(std::move(lift_all_and_ack));

    auto cancel = [this]() {
        this->stopLongPressTimer();
        this->handleCancelledGesture();
    };

    this->addTouchGesture(std::make_unique<wf::touch::gesture_t>(std::move(long_press_actions), []() {}, cancel));
}

void IGestureManager::addEdgeSwipeGesture(
    const float* sensitivity, const int64_t* timeout, const long int* edge_margin
) {
    auto edge            = std::make_unique<CMultiAction>(SWIPE_INCORRECT_DRAG_TOLERANCE, sensitivity, timeout);
    auto edge_ptr        = edge.get();
    auto edge_drag_begin = std::make_unique<OnCompleteAction>(std::move(edge), [=, this](uint32_t time) {
        auto origin_edges = this->find_swipe_edges(m_sGestureState.get_center().origin, *edge_margin);

        if (origin_edges == 0) {
            return;
        }
        auto direction = edge_ptr->target_direction;
        auto gesture   = DragGestureEvent{
              .time         = time,
              .type         = DragGestureType::EDGE_SWIPE,
              .direction    = direction,
              .finger_count = static_cast<uint32_t>(edge_ptr->finger_count),
              .edge_origin  = origin_edges
        };
        if (this->emitDragGesture(gesture)) {
            this->cancelTouchEventsOnAllWindows();
        }
    });
    auto release_and_ack = std::make_unique<OnCompleteAction>(
        std::make_unique<wf::touch::touch_action_t>(1, false),
        [edge_ptr, edge_margin, this](uint32_t time) {
            auto origin_edges = find_swipe_edges(m_sGestureState.get_center().origin, *edge_margin);
            auto direction    = edge_ptr->target_direction;
            auto dragEvent    = DragGestureEvent{
                   .time         = time,
                   .type         = DragGestureType::EDGE_SWIPE,
                   .direction    = direction,
                   .finger_count = static_cast<uint32_t>(edge_ptr->finger_count),
                   .edge_origin  = origin_edges,
            };

            if (this->emitDragGestureEnd(dragEvent)) {
                return;
            }

            if (origin_edges == 0) {
                return;
            }

            auto event = CompletedGestureEvent{
                CompletedGestureType::EDGE_SWIPE, direction, edge_ptr->finger_count, origin_edges
            };

            this->emitCompletedGesture(event);
        }
    );
    // TODO do I really need this:
    // edge->set_move_tolerance(SWIPE_INCORRECT_DRAG_TOLERANCE * *sensitivity);

    // The release action needs longer duration to handle the case where the
    // gesture is actually longer than the max distance.
    // TODO make this adjustable:
    // edge_release->set_duration(GESTURE_BASE_DURATION * 1.5 * *sensitivity);

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> edge_swipe_actions;
    edge_swipe_actions.emplace_back(std::move(edge_drag_begin));
    edge_swipe_actions.emplace_back(std::move(release_and_ack));

    auto cancel = [this]() { this->handleCancelledGesture(); };

    auto gesture = std::make_unique<wf::touch::gesture_t>(std::move(edge_swipe_actions), []() {}, cancel);
    this->addTouchGesture(std::move(gesture));
}

// TODO: timeouts (also in other gestures)
void IGestureManager::addPinchGesture(const float* threshold, const int64_t* timeout) {
    auto pinch_begin = std::make_unique<PinchAction>(threshold);

    auto pinch_wrapper = std::make_unique<OnCompleteAction>(std::move(pinch_begin), [this](uint32_t time) {
        PinchDirection dir = this->m_sGestureState.get_pinch_scale() < 1.0 ? PinchDirection::IN : PinchDirection::OUT;

        auto gesture = DragGestureEvent{
            .time            = time,
            .type            = DragGestureType::PINCH,
            .direction       = 0,
            .finger_count    = static_cast<uint32_t>(this->m_sGestureState.fingers.size()),
            .edge_origin     = 0,
            .pinch_direction = dir,
        };
        if (this->emitDragGesture(gesture)) {
            this->cancelTouchEventsOnAllWindows();
        }
    });
    auto release       = std::make_unique<LiftoffAction>();

    std::vector<std::unique_ptr<wf::touch::gesture_action_t>> pinch_actions;
    pinch_actions.emplace_back(std::move(pinch_wrapper));
    pinch_actions.emplace_back(std::move(release));

    auto ack = [this]() {
        if (!this->activeDragGesture.has_value()) {
            auto pinch_direction =
                this->m_sGestureState.get_pinch_scale() < 1.0 ? PinchDirection::IN : PinchDirection::OUT;
            auto event = CompletedGestureEvent{
                CompletedGestureType::PINCH, 0, static_cast<int>(this->m_sGestureState.fingers.size()), 0,
                pinch_direction
            };

            if (this->emitCompletedGesture(event)) {
                this->cancelTouchEventsOnAllWindows();
            }
            return;
        }

        auto active = this->activeDragGesture.value();
        if (this->emitDragGestureEnd(active)) {
            return;
        }
    };
    auto cancel = [this]() { this->handleCancelledGesture(); };

    auto gesture = std::make_unique<wf::touch::gesture_t>(std::move(pinch_actions), ack, cancel);
    this->addTouchGesture(std::move(gesture));
}
