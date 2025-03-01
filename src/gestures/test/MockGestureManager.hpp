#pragma once
#include "../Gestures.hpp"
#include "CoutLogger.hpp"
#include "wayfire/touch/touch.hpp"
#include <memory>
#include <vector>

constexpr double MONITOR_WIDTH  = 1920;
constexpr double MONITOR_HEIGHT = 1080;

class Tester {
  public:
    static void testFindSwipeEdges();
};

class CMockGestureManager final : public IGestureManager {
  public:
    CMockGestureManager(bool handlesDragEvents)
        : IGestureManager(std::make_unique<CoutLogger>()), handlesDragEvents(handlesDragEvents) {}
    ~CMockGestureManager() {}

    // if set to true, handleDragGesture() will return true
    bool handlesDragEvents;

    bool triggered = false;
    bool cancelled = false;
    bool dragEnded = false;

    struct {
        double w, h;
    } mon_size = {MONITOR_WIDTH, MONITOR_HEIGHT};

    // creates a gesture manager that handles all drag gestures
    static CMockGestureManager newDragHandler() {
        return CMockGestureManager(true);
    }

    // creates a gesture manager that ignores drag gesture events
    static CMockGestureManager newCompletedGestureOnlyHandler() {
        return CMockGestureManager(false);
    }

    void resetTestResults() {
        triggered = false;
        cancelled = false;
        dragEnded = false;
    }

    auto getGestureAt(int index) const {
        return &this->m_vGestures.at(index);
    }

    wf::touch::point_t getLastPositionOfFinger(int id) {
        auto pos = &this->m_sGestureState.fingers[id].current;
        return {pos->x, pos->y};
    }

    bool handleCompletedGesture(const CompletedGestureEvent& gev) override;
    bool handleDragGesture(const DragGestureEvent& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGestureEvent& gev) override;
    void handleCancelledGesture() override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override {}
    void stopLongPressTimer() override {}

  protected:
    wf::touch::point_t getMonitorSize() const override {
        return wf::touch::point_t{this->mon_size.w, this->mon_size.h};
    };

  private:
    void sendCancelEventsToWindows() override;
    friend Tester;
};
