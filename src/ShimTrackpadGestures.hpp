#include "gestures/DragGesture.hpp"
#include "src/managers/input/trackpad/GestureTypes.hpp"
#include <any>
#include <cstddef>
#include <cstdint>
#include <string>

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/string/VarList.hpp>

#define private public
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#undef private

constexpr size_t MOD_MASK_SHIFT = 8;
constexpr size_t FINGERS_MASK   = 0xFF; // lowest 8 bits

struct GestureConfig {
    DragGestureType type;
    eTrackpadGestureDirection direction;
    size_t fingersOrOrigin;
    // bit mask of eKeyboardModifiers
    uint32_t modMask = 0;

    inline size_t fingers() const {
        return fingersOrOrigin;
    }

    static eTrackpadGestureDirection originFromFingers(size_t);
};

std::expected<GestureConfig, std::string> parseGesturePattern(Hyprutils::String::CConstVarList& vars);
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
        return &gestures[size_t(DragGestureType::SWIPE)];
    }
    inline CTrackpadGestures* edge() {
        return &gestures[size_t(DragGestureType::EDGE_SWIPE)];
    }
    inline CTrackpadGestures* longPress() {
        return &gestures[size_t(DragGestureType::LONG_PRESS)];
    }
    inline CTrackpadGestures* pinch() {
        return &gestures[size_t(DragGestureType::PINCH)];
    }

    inline CTrackpadGestures* get(DragGestureType type) {
        if (0 <= size_t(type) && size_t(type) < sizeof(gestures) / sizeof(CTrackpadGestures))
            return &gestures[size_t(type)];

        return nullptr;
    }

    // maybe making a function returning std::array would be better? idk
    CTrackpadGestures gestures[4];

    void listGestures();

    static bool isPinch(eTrackpadGestureDirection dir);
    static bool isSingleDirection(eTrackpadGestureDirection dir);
};

inline std::unique_ptr<ShimTrackpadGestures> g_pShimTrackpadGestures;
