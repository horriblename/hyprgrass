#pragma once
#include "../Gestures.hpp"
#include "CoutLogger.hpp"
#include "wayfire/touch/touch.hpp"
#include <memory>
#include <vector>

constexpr double MONITOR_X      = 0;
constexpr double MONITOR_Y      = 0;
constexpr double MONITOR_WIDTH  = 1920;
constexpr double MONITOR_HEIGHT = 1080;

class CMockGestureManager final : public IGestureManager {
  public:
    CMockGestureManager(FindGestureResult completedEventsResult, bool handlesDragEvents)
        : IGestureManager(std::make_unique<CoutLogger>()), completedEventsResult(completedEventsResult),
          handlesDragEvents(handlesDragEvents) {}
    ~CMockGestureManager() {}

    FindGestureResult completedEventsResult;
    bool handlesDragEvents;

    bool triggered        = false;
    bool cancelled        = false;
    bool dragEnded        = false;
    bool sentWindowCancel = false;

    struct {
        double x, y;
    } mon_offset = {MONITOR_X, MONITOR_Y};

    struct {
        double w, h;
    } mon_size = {MONITOR_WIDTH, MONITOR_HEIGHT};

    // creates a gesture manager that handles all drag gestures
    static CMockGestureManager newDragHandler() {
        return CMockGestureManager(FindGestureResult::NONE, true);
    }

    // creates a gesture manager that ignores drag gesture events
    static CMockGestureManager newCompletedGestureOnlyHandler() {
        return CMockGestureManager(FindGestureResult::FOUND, false);
    }

    // creates a gesture manager that handles both completed and drag events
    static CMockGestureManager newBothHandler() {
        return CMockGestureManager(FindGestureResult::FOUND, true);
    }

    // creates a gesture manager that executes gestures but does not consume
    // them, i.e. touch events are still forwarded to windows
    static CMockGestureManager newNonConsumingHandler() {
        return CMockGestureManager(FindGestureResult::NON_CONSUMING, false);
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

    FindGestureResult findCompletedGesture(const CompletedGestureEvent& gev) const override;
    FindGestureResult handleCompletedGesture(const CompletedGestureEvent& gev) override;
    bool handleDragGesture(const DragGestureEvent& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGestureEvent& gev) override;
    void handleCancelledGesture() override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override {}
    void stopLongPressTimer() override {}

  protected:
    SMonitorArea getMonitorArea() const override {
        return SMonitorArea{this->mon_offset.x, this->mon_offset.y, this->mon_size.w, this->mon_size.h};
    }

  private:
    void sendCancelEventsToWindows() override;
};
