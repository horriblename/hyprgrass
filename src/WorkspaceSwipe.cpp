#include "WorkspaceSwipe.hpp"
#include "globals.hpp"
#include <cassert>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

// Begin a workspace swipe action
// @param pos_in_percent is the position of the center of the swipe, must be
// 0..1 in units of "percentage of the screen size"
void WorkspaceSwipeManager::beginSwipe(
    uint32_t time, const wf::touch::point_t& pos_in_percent) {
    assert(0 <= pos_in_percent.x && pos_in_percent.x <= 1);
    assert(0 <= pos_in_percent.y && pos_in_percent.y <= 1);
    static auto* const PSWIPEFINGERS =
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "gestures:workspace_swipe_fingers")
             ->intValue;

    // HACK .pointer is not used by g_pInputManager->onSwipeBegin so it's fine I
    // think
    auto emulated_swipe =
        wlr_pointer_swipe_begin_event{.pointer   = nullptr,
                                      .time_msec = time,
                                      .fingers   = (uint32_t)*PSWIPEFINGERS};
    g_pInputManager->onSwipeBegin(&emulated_swipe);

    m_lastPosInPercent = pos_in_percent;
    m_active           = true;
}

void WorkspaceSwipeManager::endSwipe(uint32_t time, bool cancelled) {
    assert(m_active);

    auto emulated_swipe = wlr_pointer_swipe_end_event{
        .pointer = nullptr, .time_msec = time, .cancelled = cancelled};
    g_pInputManager->onSwipeEnd(&emulated_swipe);

    m_active = false;
}

// @param pos_in_percent is the position of the center of the swipe, must be
// 0..1 in units of "percentage of the screen size"
void WorkspaceSwipeManager::moveSwipe(
    uint32_t time, const wf::touch::point_t& pos_in_percent) {
    assert(0 <= pos_in_percent.x && pos_in_percent.x <= 1);
    assert(0 <= pos_in_percent.y && pos_in_percent.y <= 1);
    assert(m_active);

    static auto* const PSWIPEDIST =
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "gestures:workspace_swipe_distance")
             ->intValue;
    static auto* const PSWIPEFINGERS =
        &HyprlandAPI::getConfigValue(PHANDLE,
                                     "gestures:workspace_swipe_fingers")
             ->intValue;

    if (!g_pInputManager->m_sActiveSwipe.pMonitor) {
        Debug::log(
            ERR, "ignoring touch gesture motion event due to missing monitor!");
        return;
    }

    // touch coords are within 0 to 1, we need to scale it with PSWIPEDIST for
    // one to one gesture
    auto emulated_swipe = wlr_pointer_swipe_update_event{
        .pointer   = nullptr,
        .time_msec = time,
        .fingers   = (uint32_t)*PSWIPEFINGERS,
        .dx        = (pos_in_percent.x - m_lastPosInPercent.x) * *PSWIPEDIST,
        .dy        = (pos_in_percent.y - m_lastPosInPercent.y) * *PSWIPEDIST};

    g_pInputManager->onSwipeUpdate(&emulated_swipe);
    m_lastPosInPercent = pos_in_percent;
}

bool WorkspaceSwipeManager::isActive() const {
    return m_active;
}
