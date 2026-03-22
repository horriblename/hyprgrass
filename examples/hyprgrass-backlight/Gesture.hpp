#include "src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"
#include <hyprutils/math/Vector2D.hpp>

class BacklightGesture : public ITrackpadGesture {
  public:
    BacklightGesture() {}

    void begin(const STrackpadGestureBegin& e) override;
    void update(const STrackpadGestureUpdate& e) override;
    void end(const STrackpadGestureEnd& e) override;

    bool isDirectionSensitive() override;
};

struct GlobalState {
    Vector2D last_triggered_pos = {0, 0};
    Vector2D accumulated_delta  = {0, 0};
};
inline std::unique_ptr<GlobalState> g_pGlobalState;
