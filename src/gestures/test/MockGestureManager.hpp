#pragma once
#include "../Gestures.hpp"
#include "wayfire/touch/touch.hpp"
#include <vector>
#include <wlr/types/wlr_touch.h>

class CMockGestureManager : public IGestureManager {
  public:
    CMockGestureManager();
    ~CMockGestureManager() {}

    bool triggered = false;
    bool cancelled = false;
    void addWorkspaceSwipeBeginGesture();

  protected:
    std::optional<SMonitorArea> getMonitorArea() override {
        return SMonitorArea{0, 0, 1080, 1920};
    }

  private:
    void resetTestResults() {
        triggered = false;
        cancelled = false;
    }
};
