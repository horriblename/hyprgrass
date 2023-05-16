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
    bool testFindSwipeEdges();
};

class CMockGestureManager : public IGestureManager {
  public:
    CMockGestureManager();
    ~CMockGestureManager() {}

    bool triggered = false;
    bool cancelled = false;
    void resetTestResults() {
        triggered = false;
        cancelled = false;
    }

    auto getGestureAt(int index) const {
        return &this->m_vGestures.at(index);
    }

    wf::touch::point_t getLastPositionOfFinger(int id) {
        auto pos = &this->m_sGestureState.fingers[id].current;
        return {pos->x, pos->y};
    }

    void handleGesture(const TouchGesture& gev) override;
    void handleCancelledGesture() override;

  protected:
    SMonitorArea getMonitorArea() const override {
        return SMonitorArea{MONITOR_X, MONITOR_Y, MONITOR_WIDTH,
                            MONITOR_HEIGHT};
    }

  private:
    friend Tester;
};
