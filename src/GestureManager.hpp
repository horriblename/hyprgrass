#pragma once
#include "./gestures/Gestures.hpp"
#include "ShimTrackpadGestures.hpp"
#include "VecSet.hpp"

#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#define private public
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/lua/ConfigManager.hpp>
#include <hyprland/src/config/lua/LuaRef.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/keybinds/Manager.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>
#undef private

enum class GestureEventType {
    DRAG_BEGIN,
    DRAG_END,
    COMPLETED, // CompletedGestureEvent
};

// compat adapter
struct SKeybind {
    std::string key;
    std::string handler;
    std::string arg;
    // only set for handler == "__lua": the dispatcher function to call
    Config::Lua::CLuaRef luaFn;
    std::string displayKey;
    bool mouse        = false;
    bool locked       = false;
    bool nonConsuming = false;
    uint32_t modmask  = 0;
};

struct Cfg {
    SP<Config::Values::CIntValue> workspaceSwipeFingers, longPressDelay, edgeMargin;
    SP<Config::Values::CStringValue> workspaceSwipeEdge;
    SP<Config::Values::CFloatValue> sensitivity;
    SP<Config::Values::CBoolValue> sendCancel, resizeOnBorder;

    using INT   = Config::Values::CIntValue;
    using STR   = Config::Values::CStringValue;
    using FLOAT = Config::Values::CFloatValue;
    using BOOL  = Config::Values::CBoolValue;
    Cfg()
        : // config options
          workspaceSwipeFingers{makeShared<INT>(
              "plugin:hyprgrass:workspace_swipe_fingers", "Number of fingers to trigger workspace swipe", -1
          )},
          longPressDelay{makeShared<INT>("plugin:hyprgrass:long_press_delay", "Long press delay in milliseconds", 400)},
          edgeMargin{makeShared<INT>(
              "plugin:hyprgrass:edge_margin", "Distance from edge of screen to consider for edge gestures, in pixels",
              10
          )},
          workspaceSwipeEdge{makeShared<STR>(
              "plugin:hyprgrass:workspace_swipe_edge", "Edge to swipe from to trigger workspace swipe", ""
          )},
          sensitivity{makeShared<FLOAT>("plugin:hyprgrass:sensitivity", "Gesture sensitivity", 1.0)},
          sendCancel{makeShared<BOOL>(
              "plugin:hyprgrass:debug:send_cancel", "Whether to send cancel events to windows on gesture activation",
              true
          )},
          resizeOnBorder{makeShared<BOOL>(
              "plugin:hyprgrass:resize_on_border_long_press", "Resize window by pressing and holding on borders", true
          )} {}
};

class GestureManager final : public IGestureManager {
  public:
    uint32_t long_press_next_trigger_time;
    std::vector<SP<SKeybind>> internalBinds;

    GestureManager();
    ~GestureManager();
    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchDown(ITouch::SDownEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchUp(ITouch::SUpEvent e);

    // @return whether this touch event should be blocked from forwarding to the
    // client window/surface
    bool onTouchMove(ITouch::SMotionEvent e);

    void onLongPressTimeout(uint32_t time_msec);

    const wf::touch::gesture_state_t& getGestureState() const {
        return this->m_sGestureState;
    }
    SMonitorArea getMonitorArea() const override;

  protected:
    FindGestureResult findCompletedGesture(const CompletedGestureEvent& gev) const override;
    FindGestureResult handleCompletedGesture(const CompletedGestureEvent& gev) override;
    void handleCancelledGesture() override;

    void debugLog(const std::string& msg) override;

  private:
    VecSet<CWeakPointer<CWLTouchResource>> touchedResources;
    PHLMONITOR m_lastTouchedMonitor;
    SMonitorArea m_monitorArea;
    wl_event_source* long_press_timer;
    struct {
        bool active = false;
        Config::CCssGapData old_gaps_in;
    } resizeOnBorderInfo;
    bool workspaceSwipeActive                = false;
    CTrackpadGestures* activeTrackpadGesture = nullptr;
    bool mouseBindActive                     = false;
    // used by trackpadGesture* functions
    wf::touch::point_t emulatedSwipePoint;

    FindGestureResult handleGestureBind(std::string bind, GestureEventType);

    // converts wlr touch event positions (number between 0.0 to 1.0) to pixel position,
    // takes into consideration monitor size and offset
    wf::touch::point_t wlrTouchEventPositionAsPixels(double x, double y) const;
    // reverse of wlrTouchEventPositionAsPixels
    Vector2D pixelPositionToPercentagePosition(wf::touch::point_t) const;
    Vector2D pixelToTrackpadDistance(wf::touch::point_t) const;
    bool handleWorkspaceSwipe(const GestureDirection direction);
    void updateWorkspaceSwipe();

    bool trackpadGestureBegin(const DragGestureEvent& gev);
    void trackpadGestureUpdate(uint32_t time);
    void trackpadGestureEnd(uint32_t time);

    bool handleDragGesture(const DragGestureEvent& gev) override;
    void dragGestureUpdate(const wf::touch::gesture_event_t&) override;
    void handleDragGestureEnd(const DragGestureEvent& gev) override;

    void updateLongPressTimer(uint32_t current_time, uint32_t delay) override;
    void stopLongPressTimer() override;

    void sendCancelEventsToWindows() override;

    FindGestureResult findGestureBind(std::string bind, GestureEventType type) const;
};

inline std::unique_ptr<GestureManager> g_pGestureManager;
inline UP<Cfg> g_config;
