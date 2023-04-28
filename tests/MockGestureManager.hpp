#pragma once
#include "../src/Gestures.hpp"
#include "wayfire/touch/touch.hpp"
#include <vector>
#include <wlr/types/wlr_touch.h>

class CMockGestureManager : public IGestureManager {
  public:
    CMockGestureManager();
    ~CMockGestureManager() {}

    bool workspaceSwipeTriggered = false;
    bool workspaceSwipeCancelled = false;

  private:
    void resetTestResults() {
        workspaceSwipeTriggered = false;
        workspaceSwipeCancelled = false;
    }
};
