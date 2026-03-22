#pragma once

#include <cstdint>

#include "GestureManager.hpp"
#include "src/devices/IPointer.hpp"
#include "src/managers/input/InputManager.hpp"
#include "src/managers/input/trackpad/GestureTypes.hpp"
#include "src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"

class EmulateTouchpadGesture : public ITrackpadGesture {
  public:
    EmulateTouchpadGesture(uint32_t fingers, eTrackpadGestureDirection dir) : fingers(fingers), direction(dir) {}

    void begin(const STrackpadGestureBegin& e) override {
        if (ShimTrackpadGestures::isPinch(this->direction) != ShimTrackpadGestures::isPinch(e.direction)) {
            return;
        }

        uint32_t time = e.swipe ? e.swipe->timeMs : e.pinch->timeMs;
        if (ShimTrackpadGestures::isPinch(this->direction)) {
            IPointer::SPinchBeginEvent pinch = {
                .timeMs  = time,
                .fingers = this->fingers,
            };
            g_pInputManager->onPinchBegin(pinch);
        } else {
            IPointer::SSwipeBeginEvent swipe = {
                .timeMs  = time,
                .fingers = this->fingers,
            };
            g_pInputManager->onSwipeBegin(swipe);
        }
    };

    void update(const STrackpadGestureUpdate& e) override {
        if (ShimTrackpadGestures::isPinch(this->direction) != ShimTrackpadGestures::isPinch(e.direction)) {
            return;
        }

        uint32_t time = e.swipe ? e.swipe->timeMs : e.pinch->timeMs;
        if (ShimTrackpadGestures::isPinch(this->direction)) {
            IPointer::SPinchUpdateEvent pinch = {
                .timeMs  = time,
                .fingers = this->fingers,
                .delta   = e.pinch->delta,
            };
            g_pInputManager->onPinchUpdate(pinch);
        } else {
            IPointer::SSwipeUpdateEvent swipe = {
                .timeMs  = e.swipe->timeMs,
                .fingers = e.swipe->fingers,
                .delta   = e.swipe->delta,
            };
            g_pInputManager->onSwipeUpdate(swipe);
        }
    }
    void end(const STrackpadGestureEnd& e) override {
        if (ShimTrackpadGestures::isPinch(this->direction) != ShimTrackpadGestures::isPinch(e.direction)) {
            return;
        }

        if (auto s = e.swipe) {
            IPointer::SSwipeEndEvent swipe = {
                .timeMs    = s->timeMs,
                .cancelled = s->cancelled,
            };
            g_pInputManager->onSwipeEnd(swipe);
        } else if (auto p = e.pinch) {
            IPointer::SPinchEndEvent pinch = {
                .timeMs    = p->timeMs,
                .cancelled = p->cancelled,
            };
            g_pInputManager->onPinchEnd(pinch);
        }
    };

  private:
    uint32_t fingers;
    eTrackpadGestureDirection direction;
};
