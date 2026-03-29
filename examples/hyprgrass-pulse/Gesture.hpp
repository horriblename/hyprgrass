#include "src/helpers/memory/Memory.hpp"
#include "src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"

class PulseGesture : public ITrackpadGesture {
  public:
    PulseGesture() {}

    void begin(const STrackpadGestureBegin& e) override;
    void update(const STrackpadGestureUpdate& e) override;
    void end(const STrackpadGestureEnd& e) override;

    bool isDirectionSensitive() override;
};

struct GlobalState {
    float accumulated_delta = 0.0;
};
inline UP<GlobalState> g_pGlobalState;
