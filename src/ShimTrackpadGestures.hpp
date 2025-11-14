#include "gestures/DragGesture.hpp"
#include <string>

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprutils/string/ConstVarList.hpp>

#define private public
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#undef private

struct GestureConfig {
    DragGestureType type;
    eTrackpadGestureDirection direction;
    size_t fingers;
};

std::expected<GestureConfig, std::string> parseGesturePattern(CConstVarList& vars);
GestureDirection toHyprgrassDirection(eTrackpadGestureDirection dir);

struct ShimTrackpadGestures {
  public:
    ShimTrackpadGestures() {
        gestures[0] = CTrackpadGestures();
        gestures[1] = CTrackpadGestures();
        gestures[2] = CTrackpadGestures();
        gestures[3] = CTrackpadGestures();
    }

    inline CTrackpadGestures* swipe() {
        return &gestures[0];
    }
    inline CTrackpadGestures* edge() {
        return &gestures[1];
    }
    inline CTrackpadGestures* longPress() {
        return &gestures[2];
    }
    inline CTrackpadGestures* pinch() {
        return &gestures[3];
    }

    inline CTrackpadGestures* get(DragGestureType type) {
        switch (type) {
            case DragGestureType::SWIPE:
                return this->swipe();
            case DragGestureType::LONG_PRESS:
                return this->longPress();
            case DragGestureType::EDGE_SWIPE:
                return this->edge();
            case DragGestureType::PINCH:
                return this->pinch();
        }
        return nullptr;
    }

    // maybe making a function returning std::array would be better? idk
    CTrackpadGestures gestures[4];
};

inline std::unique_ptr<ShimTrackpadGestures> g_pShimTrackpadGestures;
