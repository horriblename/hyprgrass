#pragma once
#include "../Gestures.hpp"
#include "wayfire/touch/touch.hpp"
#include <vector>

class CMockGestureManager : public IGestureManager {
  public:
    CMockGestureManager();
    ~CMockGestureManager() {}

    bool triggered = false;
    bool cancelled = false;
    void addWorkspaceSwipeBeginGesture();
    void addEdgeSwipeGesture();
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

  protected:
    SMonitorArea getMonitorArea() const override {
        return SMonitorArea{0, 0, 1080, 1920};
    }

  private:
};
