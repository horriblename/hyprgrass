#pragma once
#include "../Gestures.hpp"
#include "wayfire/touch/touch.hpp"
#include <vector>

constexpr double MONITOR_X      = 0;
constexpr double MONITOR_Y      = 0;
constexpr double MONITOR_WIDTH  = 1920;
constexpr double MONITOR_HEIGHT = 1080;

class Tester {
  public:
    static void testFindSwipeEdges();
};

class CMockGestureManager final : public IGestureManager {
  public:
    CMockGestureManager();
    ~CMockGestureManager() {}

    // if set to true, handleDragGesture() will return true
    bool handlesDragEvents = false;

    bool triggered = false;
    bool cancelled = false;
    bool dragEnded = false;

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

    bool handleCompletedGesture(const CompletedGesture& gev) override;
    bool handleDragGesture(const DragGesture& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGesture& gev) override;
    void handleCancelledGesture() override;

  protected:
    SMonitorArea getMonitorArea() const override {
        return SMonitorArea{MONITOR_X, MONITOR_Y, MONITOR_WIDTH, MONITOR_HEIGHT};
    }

  private:
    void sendCancelEventsToWindows() override;
    friend Tester;
};
